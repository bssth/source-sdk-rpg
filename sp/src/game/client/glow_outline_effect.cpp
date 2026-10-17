//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Functionality to render a glowing outline around client renderable objects.
//
// Pipeline (no custom shaders required):
//   1. Mark all glow entities into the stencil buffer (alpha 0, depth-aware).
//   2. Render the same entities into _rt_FullFrameFB tinted with their glow
//      colors via the dev/glow_color override material.
//   3. Downsample _rt_FullFrameFB into _rt_SmallFB0 with dev/downsample_non_hdr.
//   4. Gaussian blur X with dev/blurfilterx_nohdr  ( _rt_SmallFB0 -> _rt_SmallFB1 ).
//   5. Gaussian blur Y with dev/blurfiltery_nohdr  ( _rt_SmallFB1 -> _rt_SmallFB0 ).
//   6. Composite _rt_SmallFB0 to the back buffer additively with
//      dev/glow_composite, but only where stencil == 0 (i.e. outside the
//      original silhouettes), which produces the L4D-style outline.
//
// Steps 3-5 reuse the same engine-shipped materials the bloom pass uses, so we
// don't need to compile any new shaders.
//
//===============================================================================

#include "cbase.h"
#include "glow_outline_effect.h"
#include "model_types.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "view_shared.h"
#include "viewpostprocess.h"
#include "clienteffectprecachesystem.h"

#define FULL_FRAME_TEXTURE "_rt_FullFrameFB"

#ifdef GLOWS_ENABLE

// Hold references so the materials found below aren't bound with a zero
// refcount ("bad reference count 0 when being bound" spam).
CLIENTEFFECT_REGISTER_BEGIN( PrecacheGlowOutlineEffect )
CLIENTEFFECT_MATERIAL( "dev/glow_color" )
CLIENTEFFECT_MATERIAL( "dev/glow_composite" )
CLIENTEFFECT_REGISTER_END()

ConVar glow_outline_effect_enable( "glow_outline_effect_enable", "1", FCVAR_ARCHIVE, "Enable entity outline glow effects." );
ConVar glow_outline_effect_width( "glow_outline_width", "10.0f", FCVAR_CHEAT, "Width of glow outline effect in screen space." );

extern bool g_bDumpRenderTargets; // in viewpostprocess.cpp

CGlowObjectManager g_GlowObjectManager;

struct ShaderStencilState_t
{
	bool m_bEnable;
	StencilOperation_t m_FailOp;
	StencilOperation_t m_ZFailOp;
	StencilOperation_t m_PassOp;
	StencilComparisonFunction_t m_CompareFunc;
	int m_nReferenceValue;
	uint32 m_nTestMask;
	uint32 m_nWriteMask;

	ShaderStencilState_t()
	{
		m_bEnable = false;
		m_PassOp = m_FailOp = m_ZFailOp = STENCILOPERATION_KEEP;
		m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
		m_nReferenceValue = 0;
		m_nTestMask = m_nWriteMask = 0xFFFFFFFF;
	}

	void SetStencilState( CMatRenderContextPtr &pRenderContext )
	{
		pRenderContext->SetStencilEnable( m_bEnable );
		pRenderContext->SetStencilFailOperation( m_FailOp );
		pRenderContext->SetStencilZFailOperation( m_ZFailOp );
		pRenderContext->SetStencilPassOperation( m_PassOp );
		pRenderContext->SetStencilCompareFunction( m_CompareFunc );
		pRenderContext->SetStencilReferenceValue( m_nReferenceValue );
		pRenderContext->SetStencilTestMask( m_nTestMask );
		pRenderContext->SetStencilWriteMask( m_nWriteMask );
	}
};

void CGlowObjectManager::RenderGlowEffects( const CViewSetup *pSetup, int nSplitScreenSlot )
{
	if ( !g_pMaterialSystemHardwareConfig->SupportsPixelShaders_2_0() )
		return;

	if ( !glow_outline_effect_enable.GetBool() )
		return;

	// Quick bail if there is nothing to glow this frame.
	bool bAny = false;
	for ( int i = 0; i < m_GlowObjectDefinitions.Count(); ++i )
	{
		if ( !m_GlowObjectDefinitions[i].IsUnused() && m_GlowObjectDefinitions[i].ShouldDraw( nSplitScreenSlot ) )
		{
			bAny = true;
			break;
		}
	}
	if ( !bAny )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	int nX, nY, nWidth, nHeight;
	pRenderContext->GetViewport( nX, nY, nWidth, nHeight );

	PIXEvent _pixEvent( pRenderContext, "EntityGlowEffects" );
	ApplyEntityGlowEffects( pSetup, nSplitScreenSlot, pRenderContext, glow_outline_effect_width.GetFloat(), nX, nY, nWidth, nHeight );
}

static void SetRenderTargetAndViewPort( ITexture *rt, int w, int h )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetRenderTarget( rt );
	pRenderContext->Viewport( 0, 0, w, h );
}

void CGlowObjectManager::RenderGlowModels( const CViewSetup *pSetup, int nSplitScreenSlot, CMatRenderContextPtr &pRenderContext )
{
	//==========================================================================================//
	// This renders solid pixels with the correct coloring for each object that needs the glow.	//
	// After this function returns, this image will then be blurred and added into the frame	//
	// buffer with the objects stenciled out.													//
	//==========================================================================================//
	pRenderContext->PushRenderTargetAndViewport();

	// Save modulation color and blend
	Vector vOrigColor;
	render->GetColorModulation( vOrigColor.Base() );
	float flOrigBlend = render->GetBlend();

	// Get pointer to FullFrameFB
	ITexture *pRtFullFrame = materials->FindTexture( FULL_FRAME_TEXTURE, TEXTURE_GROUP_RENDER_TARGET );

	SetRenderTargetAndViewPort( pRtFullFrame, pSetup->width, pSetup->height );

	pRenderContext->ClearColor3ub( 0, 0, 0 );
	pRenderContext->ClearBuffers( true, false, false );

	// Set override material for glow color
	IMaterial *pMatGlowColor = materials->FindMaterial( "dev/glow_color", TEXTURE_GROUP_OTHER, true );
	g_pStudioRender->ForcedMaterialOverride( pMatGlowColor );

	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = false;
	stencilState.m_nReferenceValue = 0;
	stencilState.m_nTestMask = 0xFF;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
	stencilState.m_PassOp = STENCILOPERATION_KEEP;
	stencilState.m_FailOp = STENCILOPERATION_KEEP;
	stencilState.m_ZFailOp = STENCILOPERATION_KEEP;

	stencilState.SetStencilState( pRenderContext );

	//==================//
	// Draw the objects //
	//==================//
	for ( int i = 0; i < m_GlowObjectDefinitions.Count(); ++i )
	{
		if ( m_GlowObjectDefinitions[i].IsUnused() || !m_GlowObjectDefinitions[i].ShouldDraw( nSplitScreenSlot ) )
			continue;

		render->SetBlend( m_GlowObjectDefinitions[i].m_flGlowAlpha );
		Vector vGlowColor = m_GlowObjectDefinitions[i].m_vGlowColor * m_GlowObjectDefinitions[i].m_flGlowAlpha;
		render->SetColorModulation( &vGlowColor[0] ); // This only sets rgb, not alpha

		m_GlowObjectDefinitions[i].DrawModel();
	}

	if ( g_bDumpRenderTargets )
	{
		DumpTGAofRenderTarget( pSetup->width, pSetup->height, "GlowModels" );
	}

	g_pStudioRender->ForcedMaterialOverride( NULL );
	render->SetColorModulation( vOrigColor.Base() );
	render->SetBlend( flOrigBlend );

	ShaderStencilState_t stencilStateDisable;
	stencilStateDisable.m_bEnable = false;
	stencilStateDisable.SetStencilState( pRenderContext );

	pRenderContext->PopRenderTargetAndViewport();
}

void CGlowObjectManager::ApplyEntityGlowEffects( const CViewSetup *pSetup, int nSplitScreenSlot, CMatRenderContextPtr &pRenderContext, float flBloomScale, int x, int y, int w, int h )
{
	//=======================================================//
	// Render objects into stencil buffer					 //
	//=======================================================//
	// Set override shader to the same simple shader we use to render the glow models
	IMaterial *pMatGlowColor = materials->FindMaterial( "dev/glow_color", TEXTURE_GROUP_OTHER, true );
	g_pStudioRender->ForcedMaterialOverride( pMatGlowColor );

	ShaderStencilState_t stencilStateDisable;
	stencilStateDisable.m_bEnable = false;
	float flSavedBlend = render->GetBlend();

	// Set alpha to 0 so we don't touch any color pixels
	render->SetBlend( 0.0f );
	pRenderContext->OverrideDepthEnable( true, false );

	int iNumGlowObjects = 0;

	for ( int i = 0; i < m_GlowObjectDefinitions.Count(); ++i )
	{
		if ( m_GlowObjectDefinitions[i].IsUnused() || !m_GlowObjectDefinitions[i].ShouldDraw( nSplitScreenSlot ) )
			continue;

		if ( m_GlowObjectDefinitions[i].m_bRenderWhenOccluded || m_GlowObjectDefinitions[i].m_bRenderWhenUnoccluded )
		{
			if ( m_GlowObjectDefinitions[i].m_bRenderWhenOccluded && m_GlowObjectDefinitions[i].m_bRenderWhenUnoccluded )
			{
				ShaderStencilState_t stencilState;
				stencilState.m_bEnable = true;
				stencilState.m_nReferenceValue = 1;
				stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
				stencilState.m_PassOp = STENCILOPERATION_REPLACE;
				stencilState.m_FailOp = STENCILOPERATION_KEEP;
				stencilState.m_ZFailOp = STENCILOPERATION_REPLACE;

				stencilState.SetStencilState( pRenderContext );

				m_GlowObjectDefinitions[i].DrawModel();
			}
			else if ( m_GlowObjectDefinitions[i].m_bRenderWhenOccluded )
			{
				ShaderStencilState_t stencilState;
				stencilState.m_bEnable = true;
				stencilState.m_nReferenceValue = 1;
				stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
				stencilState.m_PassOp = STENCILOPERATION_KEEP;
				stencilState.m_FailOp = STENCILOPERATION_KEEP;
				stencilState.m_ZFailOp = STENCILOPERATION_REPLACE;

				stencilState.SetStencilState( pRenderContext );

				m_GlowObjectDefinitions[i].DrawModel();
			}
			else if ( m_GlowObjectDefinitions[i].m_bRenderWhenUnoccluded )
			{
				ShaderStencilState_t stencilState;
				stencilState.m_bEnable = true;
				stencilState.m_nReferenceValue = 2;
				stencilState.m_nTestMask = 0x1;
				stencilState.m_nWriteMask = 0x3;
				stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_EQUAL;
				stencilState.m_PassOp = STENCILOPERATION_INCRSAT;
				stencilState.m_FailOp = STENCILOPERATION_KEEP;
				stencilState.m_ZFailOp = STENCILOPERATION_REPLACE;

				stencilState.SetStencilState( pRenderContext );

				m_GlowObjectDefinitions[i].DrawModel();
			}
		}

		iNumGlowObjects++;
	}

	// Need to do a 2nd pass to warm stencil for objects which are rendered only when occluded
	for ( int i = 0; i < m_GlowObjectDefinitions.Count(); ++i )
	{
		if ( m_GlowObjectDefinitions[i].IsUnused() || !m_GlowObjectDefinitions[i].ShouldDraw( nSplitScreenSlot ) )
			continue;

		if ( m_GlowObjectDefinitions[i].m_bRenderWhenOccluded && !m_GlowObjectDefinitions[i].m_bRenderWhenUnoccluded )
		{
			ShaderStencilState_t stencilState;
			stencilState.m_bEnable = true;
			stencilState.m_nReferenceValue = 2;
			stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
			stencilState.m_PassOp = STENCILOPERATION_REPLACE;
			stencilState.m_FailOp = STENCILOPERATION_KEEP;
			stencilState.m_ZFailOp = STENCILOPERATION_KEEP;
			stencilState.SetStencilState( pRenderContext );

			m_GlowObjectDefinitions[i].DrawModel();
		}
	}

	pRenderContext->OverrideDepthEnable( false, false );
	render->SetBlend( flSavedBlend );
	stencilStateDisable.SetStencilState( pRenderContext );
	g_pStudioRender->ForcedMaterialOverride( NULL );

	// If there aren't any objects to glow, don't do all this other stuff
	if ( iNumGlowObjects <= 0 )
		return;

	//=============================================
	// Render the glow colors to _rt_FullFrameFB
	//=============================================
	{
		PIXEvent pixEvent( pRenderContext, "RenderGlowModels" );
		RenderGlowModels( pSetup, nSplitScreenSlot, pRenderContext );
	}

	//=========================================================================================//
	// Downsample + Gaussian blur the glow color buffer using the engine's stock bloom kernel.  //
	// _rt_FullFrameFB -> _rt_SmallFB0 -> _rt_SmallFB1 -> _rt_SmallFB0                          //
	//=========================================================================================//
	ITexture *pRtFullFrame = materials->FindTexture( FULL_FRAME_TEXTURE, TEXTURE_GROUP_RENDER_TARGET );
	ITexture *pRtSmall0    = materials->FindTexture( "_rt_SmallFB0", TEXTURE_GROUP_RENDER_TARGET );
	ITexture *pRtSmall1    = materials->FindTexture( "_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET );

	if ( !pRtFullFrame || !pRtSmall0 || !pRtSmall1 )
	{
		// Fallback path: small RTs missing on this hardware. Skip post-process.
		return;
	}

	IMaterial *pMatDownsample = materials->FindMaterial( "dev/downsample_non_hdr", TEXTURE_GROUP_OTHER, true );
	IMaterial *pMatBlurX      = materials->FindMaterial( "dev/blurfilterx_nohdr", TEXTURE_GROUP_OTHER, true );
	IMaterial *pMatBlurY      = materials->FindMaterial( "dev/blurfiltery_nohdr", TEXTURE_GROUP_OTHER, true );
	IMaterial *pMatComposite  = materials->FindMaterial( "dev/glow_composite", TEXTURE_GROUP_OTHER, true );

	const int nSrcWidth  = pRtFullFrame->GetActualWidth();
	const int nSrcHeight = pRtFullFrame->GetActualHeight();
	const int nQuarterW  = nSrcWidth / 4;
	const int nQuarterH  = nSrcHeight / 4;

	pRenderContext->PushRenderTargetAndViewport();

	// 1. Downsample _rt_FullFrameFB -> _rt_SmallFB0
	SetRenderTargetAndViewPort( pRtSmall0, nQuarterW, nQuarterH );
	pRenderContext->DrawScreenSpaceRectangle( pMatDownsample, 0, 0, nQuarterW, nQuarterH,
		0, 0, nSrcWidth - 2, nSrcHeight - 2,
		nSrcWidth, nSrcHeight );

	// 2. Gaussian blur X _rt_SmallFB0 -> _rt_SmallFB1
	SetRenderTargetAndViewPort( pRtSmall1, nQuarterW, nQuarterH );
	pRenderContext->DrawScreenSpaceRectangle( pMatBlurX, 0, 0, nQuarterW, nQuarterH,
		0, 0, nQuarterW - 1, nQuarterH - 1,
		nQuarterW, nQuarterH );

	// 3. Gaussian blur Y _rt_SmallFB1 -> _rt_SmallFB0
	SetRenderTargetAndViewPort( pRtSmall0, nQuarterW, nQuarterH );
	// blurfiltery_nohdr expects $bloomamount on its material variable
	IMaterialVar *pBloomAmountVar = pMatBlurY->FindVar( "$bloomamount", NULL );
	if ( pBloomAmountVar )
	{
		pBloomAmountVar->SetFloatValue( flBloomScale );
	}
	pRenderContext->DrawScreenSpaceRectangle( pMatBlurY, 0, 0, nQuarterW, nQuarterH,
		0, 0, nQuarterW - 1, nQuarterH - 1,
		nQuarterW, nQuarterH );

	if ( g_bDumpRenderTargets )
	{
		DumpTGAofRenderTarget( nQuarterW, nQuarterH, "GlowBlurred" );
	}

	pRenderContext->PopRenderTargetAndViewport();

	//=========================================================================================//
	// Composite the blurred glow on top of the back buffer additively, but only where stencil  //
	// is 0 -- i.e. outside the original silhouettes. This produces the L4D-style outline.      //
	//=========================================================================================//
	{
		ShaderStencilState_t stencilState;
		stencilState.m_bEnable = true;
		stencilState.m_nWriteMask = 0x0;
		stencilState.m_nTestMask = 0xFF;
		stencilState.m_nReferenceValue = 0x0;
		stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_EQUAL;
		stencilState.m_PassOp = STENCILOPERATION_KEEP;
		stencilState.m_FailOp = STENCILOPERATION_KEEP;
		stencilState.m_ZFailOp = STENCILOPERATION_KEEP;
		stencilState.SetStencilState( pRenderContext );

		pRenderContext->DrawScreenSpaceRectangle( pMatComposite, 0, 0, w, h,
			0, 0, nQuarterW - 1, nQuarterH - 1,
			pRtSmall0->GetActualWidth(),
			pRtSmall0->GetActualHeight() );

		stencilStateDisable.SetStencilState( pRenderContext );
	}
}

void CGlowObjectManager::GlowObjectDefinition_t::DrawModel()
{
	if ( m_hEntity.Get() )
	{
		m_hEntity->DrawModel( STUDIO_RENDER );
		C_BaseEntity *pAttachment = m_hEntity->FirstMoveChild();

		while ( pAttachment != NULL )
		{
			if ( !g_GlowObjectManager.HasGlowEffect( pAttachment ) && pAttachment->ShouldDraw() )
			{
				pAttachment->DrawModel( STUDIO_RENDER );
			}
			pAttachment = pAttachment->NextMovePeer();
		}
	}
}

#endif // GLOWS_ENABLE
