// Small fixes to engine.dll that can't be made in source, applied in memory
// when the client DLL loads. Every patch checks the exact code it expects
// first and stays off if engine.dll doesn't match.
//
// World batch limit: Shader_DrawChainsStatic draws the visible world surface
// groups in the order the BSP walk found them and allows 127 switches between
// world vertex buffers per batch. Past that it skips every group that needs
// another buffer and prints "Too many vertex format changes in frame, whole
// world not rendered", so faces go missing in big open views. The patch sends
// that case to the code the same loop already uses when the index buffer is
// full: flush the batch, start a new one, carry on with the same group.

#if defined( _WIN32 ) && !defined( _WIN64 ) && !defined( _X360 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define ENGINE_PATCHES_ENABLED
#endif

#include "cbase.h"
#include "engine_patches.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef ENGINE_PATCHES_ENABLED

namespace
{

struct ModuleImage_t
{
	byte *pBase;
	size_t nSize;
	byte *pText;
	size_t nTextSize;
};

bool GetModuleImage( const char *pszModule, ModuleImage_t &image )
{
	HMODULE hModule = GetModuleHandleA( pszModule );
	if ( !hModule )
		return false;

	byte *pBase = (byte *)hModule;
	const IMAGE_DOS_HEADER *pDos = (const IMAGE_DOS_HEADER *)pBase;
	if ( pDos->e_magic != IMAGE_DOS_SIGNATURE )
		return false;

	const IMAGE_NT_HEADERS32 *pNt = (const IMAGE_NT_HEADERS32 *)( pBase + pDos->e_lfanew );
	if ( pNt->Signature != IMAGE_NT_SIGNATURE || pNt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 )
		return false;

	image.pBase = pBase;
	image.nSize = pNt->OptionalHeader.SizeOfImage;
	image.pText = NULL;
	image.nTextSize = 0;

	const IMAGE_SECTION_HEADER *pSection = IMAGE_FIRST_SECTION( pNt );
	for ( int i = 0; i < pNt->FileHeader.NumberOfSections; i++, pSection++ )
	{
		if ( !V_strncmp( (const char *)pSection->Name, ".text", IMAGE_SIZEOF_SHORT_NAME ) )
		{
			image.pText = pBase + pSection->VirtualAddress;
			image.nTextSize = pSection->Misc.VirtualSize;
			break;
		}
	}

	return image.pText != NULL;
}

byte *FindBytes( byte *pStart, size_t nSize, const void *pBytes, size_t nBytes, byte *pAfter = NULL )
{
	byte *p = pAfter ? pAfter + 1 : pStart;
	byte *pEnd = pStart + nSize - nBytes;
	for ( ; p <= pEnd; p++ )
	{
		if ( !memcmp( p, pBytes, nBytes ) )
			return p;
	}
	return NULL;
}

// The one place in .text that pushes this string's address, or NULL if there
// are none or several.
byte *FindStringPush( const ModuleImage_t &image, const char *pszString )
{
	byte *pString = FindBytes( image.pBase, image.nSize, pszString, V_strlen( pszString ) + 1 );
	if ( !pString )
		return NULL;

	byte push[5] = { 0x68 };
	*(uint32 *)&push[1] = (uint32)pString;

	byte *pPush = FindBytes( image.pText, image.nTextSize, push, sizeof( push ) );
	if ( !pPush || FindBytes( image.pText, image.nTextSize, push, sizeof( push ), pPush ) )
		return NULL;

	return pPush;
}

inline byte *Rel8Target( byte *pNextInstruction, byte rel )
{
	return pNextInstruction + (signed char)rel;
}

bool WriteCode( byte *pDest, const byte *pSrc, size_t nBytes )
{
	DWORD oldProtect;
	if ( !VirtualProtect( pDest, nBytes, PAGE_EXECUTE_READWRITE, &oldProtect ) )
		return false;

	memcpy( pDest, pSrc, nBytes );
	VirtualProtect( pDest, nBytes, oldProtect, &oldProtect );
	FlushInstructionCache( GetCurrentProcess(), pDest, nBytes );
	return true;
}

class CWorldBatchPatch
{
public:
	CWorldBatchPatch() : m_pSkip( NULL ), m_bSearched( false ), m_bApplied( false ) {}

	bool Apply()
	{
		if ( m_bApplied )
			return true;
		if ( !Find() )
			return false;
		if ( !WriteCode( m_pSkip, m_Patch, sizeof( m_Patch ) ) )
			return false;

		m_bApplied = true;
		return true;
	}

	void Revert()
	{
		if ( !m_bApplied )
			return;

		// Leave it alone if someone else has rewritten these bytes since.
		if ( !memcmp( m_pSkip, m_Patch, sizeof( m_Patch ) ) )
			WriteCode( m_pSkip, m_Original, sizeof( m_Original ) );

		m_bApplied = false;
	}

	bool IsApplied() const { return m_bApplied; }
	size_t GetOffset() const { return m_pSkip - m_Image.pBase; }

private:
	bool Find()
	{
		if ( m_bSearched )
			return m_pSkip != NULL;
		m_bSearched = true;

		if ( !GetModuleImage( "engine.dll", m_Image ) )
			return false;

		// The block that skips the group:
		//   cmp byte ptr [ebp+bWarn], 0
		//   je  done
		//   push "Too many vertex format changes..."
		byte *pWarn = FindStringPush( m_Image, "Too many vertex format changes in frame, whole world not rendered\n" );
		if ( !pWarn )
			return false;

		byte *pSkip = pWarn - 6;
		if ( pSkip[0] != 0x80 || pSkip[1] != 0x7D || pSkip[3] != 0x00 || pSkip[4] != 0x74 )
			return false;

		// Only the limit check may lead there:
		//   mov  edx, [ebp+pLastMesh]
		//   cmp  [ecx+ebx*4], edx          ; g_WorldStaticMeshes[sortID] != pLastMesh
		//   je   same_mesh
		//   mov  eax, [ebp+meshCount]
		//   cmp  eax, 7Fh
		//   jge  skip
		static const byte limitCheck[] = { 0x83, 0xF8, 0x7F, 0x7D };
		byte *pLimit = NULL;
		for ( byte *p = pSkip - sizeof( limitCheck ) - 1; p > pSkip - 0x100; p-- )
		{
			if ( !memcmp( p, limitCheck, sizeof( limitCheck ) ) && Rel8Target( p + 5, p[4] ) == pSkip )
			{
				pLimit = p;
				break;
			}
		}
		if ( !pLimit )
			return false;

		byte *pMeshCheck = pLimit - 18;
		if ( pMeshCheck[0] != 0x8B || pMeshCheck[1] != 0x55 ||
			 pMeshCheck[3] != 0x39 || pMeshCheck[4] != 0x14 || pMeshCheck[5] != 0x99 ||
			 pMeshCheck[6] != 0x0F || pMeshCheck[7] != 0x84 ||
			 pMeshCheck[12] != 0x8B || pMeshCheck[13] != 0x85 )
			return false;
		byte lastMeshVar = pMeshCheck[2];

		// The index buffer full case right below it, which is a plain "break":
		//   jle  flush
		//   push "Too many faces with the same material in scene!"
		//   call DevMsg
		//   add  esp, 4
		//   jmp  after_loop
		// flush:
		//   mov  [ebp+pLastMesh], 0
		// after_loop:
		byte *pFaces = FindStringPush( m_Image, "Too many faces with the same material in scene!\n" );
		if ( !pFaces || pFaces[-2] != 0x7E )
			return false;

		byte *pFlush = Rel8Target( pFaces, pFaces[-1] );
		static const byte clearLastMesh[] = { 0xC7, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00 };
		if ( pFlush[0] != clearLastMesh[0] || pFlush[1] != clearLastMesh[1] || pFlush[2] != lastMeshVar ||
			 memcmp( pFlush + 3, clearLastMesh + 3, 4 ) )
			return false;

		byte *pJmpAfterLoop = pFaces + 5 + 6 + 3;
		if ( pFaces[5] != 0xFF || pFaces[6] != 0x15 || pFaces[11] != 0x83 || pFaces[12] != 0xC4 || pFaces[13] != 0x04 ||
			 pJmpAfterLoop[0] != 0xEB || Rel8Target( pJmpAfterLoop + 2, pJmpAfterLoop[1] ) != pFlush + sizeof( clearLastMesh ) )
			return false;

		memcpy( m_Original, pSkip, sizeof( m_Original ) );
		m_Patch[0] = 0xE9;	// jmp flush
		*(int32 *)&m_Patch[1] = (int32)( pFlush - ( pSkip + 5 ) );
		m_pSkip = pSkip;
		return true;
	}

	ModuleImage_t m_Image;
	byte *m_pSkip;
	byte m_Original[5];
	byte m_Patch[5];
	bool m_bSearched;
	bool m_bApplied;
};

CWorldBatchPatch s_WorldBatchPatch;

} // namespace

static void ApplyWorldBatchPatch( bool bEnable )
{
	if ( !bEnable )
	{
		if ( s_WorldBatchPatch.IsApplied() )
		{
			s_WorldBatchPatch.Revert();
			Msg( "World batch patch off\n" );
		}
		return;
	}

	if ( s_WorldBatchPatch.IsApplied() )
		return;

	if ( s_WorldBatchPatch.Apply() )
		Msg( "World batch patch on (engine.dll+0x%x)\n", (unsigned int)s_WorldBatchPatch.GetOffset() );
	else
		Warning( "World batch patch: engine.dll doesn't match, \"Too many vertex format changes\" will still drop faces\n" );
}

static void WorldBatchFixChanged( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	ConVarRef var( pConVar );
	ApplyWorldBatchPatch( var.GetBool() );
}

ConVar r_worldbatch_fix( "r_worldbatch_fix", "1", 0,
	"Flush the world batch instead of dropping faces when a view hits the engine's limit of 127 vertex buffer switches (\"Too many vertex format changes in frame\").",
	WorldBatchFixChanged );

void EnginePatches_Init()
{
	ApplyWorldBatchPatch( r_worldbatch_fix.GetBool() );
}

void EnginePatches_Shutdown()
{
	ApplyWorldBatchPatch( false );
}

#else

void EnginePatches_Init()
{
}

void EnginePatches_Shutdown()
{
}

#endif // ENGINE_PATCHES_ENABLED
