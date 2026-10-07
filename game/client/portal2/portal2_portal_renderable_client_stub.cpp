#include "cbase.h"
#include "portalrenderable_flatbasic.h"

#if defined( PORTAL2 ) && defined( CLIENT_DLL )

namespace
{
FlatBasicPortalRenderingMaterials_t s_StubPortalMaterials;
}

const FlatBasicPortalRenderingMaterials_t &CPortalRenderable_FlatBasic::m_Materials = s_StubPortalMaterials;

CPortalRenderable_FlatBasic::CPortalRenderable_FlatBasic( void )
	: m_pLinkedPortal( NULL ),
	  m_ptOrigin( 0.0f, 0.0f, 0.0f ),
	  m_vForward( 1.0f, 0.0f, 0.0f ),
	  m_vUp( 0.0f, 0.0f, 1.0f ),
	  m_vRight( 0.0f, 1.0f, 0.0f ),
	  m_fStaticAmount( 0.0f ),
	  m_fSecondaryStaticAmount( 0.0f ),
	  m_fOpenAmount( 0.0f ),
	  m_bIsPortal2( false )
{
}

void CPortalRenderable_FlatBasic::DrawComplexPortalMesh( const IMaterial *pMaterialOverride, float fForwardOffsetModifier )
{
	(void)pMaterialOverride;
	(void)fForwardOffsetModifier;
}

void CPortalRenderable_FlatBasic::DrawSimplePortalMesh( const IMaterial *pMaterialOverride, float fForwardOffsetModifier )
{
	(void)pMaterialOverride;
	(void)fForwardOffsetModifier;
}

void CPortalRenderable_FlatBasic::DrawRenderFixMesh( const IMaterial *pMaterialOverride, float fFrontClipDistance )
{
	(void)pMaterialOverride;
	(void)fFrontClipDistance;
}

void CPortalRenderable_FlatBasic::DrawDepthDoublerMesh( float fForwardOffsetModifier )
{
	(void)fForwardOffsetModifier;
}

void CPortalRenderable_FlatBasic::DrawPortal( void )
{
}

void CPortalRenderable_FlatBasic::DrawPreStencilMask( void )
{
}

void CPortalRenderable_FlatBasic::DrawStencilMask( void )
{
}

void CPortalRenderable_FlatBasic::DrawPostStencilFixes( void )
{
}

void CPortalRenderable_FlatBasic::RenderPortalViewToBackBuffer( CViewRender *pViewRender, const CViewSetup &cameraView )
{
	(void)pViewRender;
	(void)cameraView;
}

void CPortalRenderable_FlatBasic::RenderPortalViewToTexture( CViewRender *pViewRender, const CViewSetup &cameraView )
{
	(void)pViewRender;
	(void)cameraView;
}

void CPortalRenderable_FlatBasic::AddToVisAsExitPortal( ViewCustomVisibility_t *pCustomVisibility )
{
	(void)pCustomVisibility;
}

bool CPortalRenderable_FlatBasic::ShouldUpdatePortalView_BasedOnView( const CViewSetup &currentView, CUtlVector<VPlane> &currentComplexFrustum )
{
	(void)currentView;
	(void)currentComplexFrustum;
	return false;
}

bool CPortalRenderable_FlatBasic::ShouldUpdatePortalView_BasedOnPixelVisibility( float fScreenFilledByStencilMaskLastFrame_Normalized )
{
	(void)fScreenFilledByStencilMaskLastFrame_Normalized;
	return false;
}

bool CPortalRenderable_FlatBasic::ShouldUpdateDepthDoublerTexture( const CViewSetup &viewSetup )
{
	(void)viewSetup;
	return false;
}

void CPortalRenderable_FlatBasic::GetToolRecordingState( bool bActive, KeyValues *msg )
{
	(void)bActive;
	(void)msg;
}

void CPortalRenderable_FlatBasic::HandlePortalPlaybackMessage( KeyValues *pKeyValues )
{
	(void)pKeyValues;
}

bool CPortalRenderable_FlatBasic::DoesExitViewIntersectWaterPlane( float waterZ, int leafWaterDataID ) const
{
	(void)waterZ;
	(void)leafWaterDataID;
	return false;
}

bool CPortalRenderable_FlatBasic::WillUseDepthDoublerThisDraw( void ) const
{
	return false;
}

bool CPortalRenderable_FlatBasic::CalcFrustumThroughPortal( const Vector &ptCurrentViewOrigin, Frustum OutputFrustum )
{
	(void)ptCurrentViewOrigin;
	(void)OutputFrustum;
	return false;
}

void CPortalRenderable_FlatBasic::ClipFixToBoundingAreaAndDraw( PortalMeshPoint_t *pVerts, const IMaterial *pMaterial )
{
	(void)pVerts;
	(void)pMaterial;
}

void CPortalRenderable_FlatBasic::Internal_DrawRenderFixMesh( const IMaterial *pMaterial )
{
	(void)pMaterial;
}

void CPortalRenderable_FlatBasic::RenderFogQuad( void )
{
}

void CPortalRenderable_FlatBasic::PortalMoved( void )
{
}

#endif
