#include "../include/cylinder_bank_object.h"

#include "../include/connecting_rod.h"
#include "../include/engine_sim_application.h"
#include "../include/piston.h"

CylinderBankObject::CylinderBankObject() {
    m_bank = nullptr;
    m_head = nullptr;
}

CylinderBankObject::~CylinderBankObject() {
    /* void */
}

void CylinderBankObject::generateGeometry() {
    const double s =
        m_bank->getBore()
        / 2.0;

    const double boreSurfaceArea =
        constants::pi
        * m_bank->getBore()
        * m_bank->getBore()
        / 4.0;

    const double chamberHeight =
        m_head
            ->getCombustionChamberVolume()
        / boreSurfaceArea;

    GeometryGenerator *gen =
        m_app
            ->getGeometryGenerator();

    const float displayDepth =
        1.0f
        - static_cast<float>(
            m_bank
                ->getDisplayDepth());

    const float lineWidth =
        static_cast<float>(
            m_bank->getBore()
            * 0.1);

    const float margin =
        lineWidth
        * 0.25f;

    const float dx =
        -static_cast<float>(
            m_bank->getDy()
            * (
                margin
                + m_bank->getBore()
                    / 2
                + lineWidth
                    / 2));

    const float dy =
        static_cast<float>(
            m_bank->getDx()
            * (
                margin
                + m_bank->getBore()
                    / 2
                + lineWidth
                    / 2));

    const float top_x =
        static_cast<float>(
            m_bank->getX()
            + m_bank->getDx()
                * (
                    m_bank
                        ->getDeckHeight()
                    + chamberHeight));

    const float top_y =
        static_cast<float>(
            m_bank->getY()
            + m_bank->getDy()
                * (
                    m_bank
                        ->getDeckHeight()
                    + chamberHeight));

    const float bottom_x =
        static_cast<float>(
            m_bank->getX()
            + m_bank->getDx()
                * (
                    displayDepth
                    * m_bank
                        ->getDeckHeight()));

    const float bottom_y =
        static_cast<float>(
            m_bank->getY()
            + m_bank->getDy()
                * (
                    displayDepth
                    * m_bank
                        ->getDeckHeight()));

    GeometryGenerator::Line2dParameters
        params;

    params.lineWidth =
        lineWidth;

    GeometryGenerator::Circle2dParameters
        circleParams;

    circleParams.radius =
        lineWidth
        / 2.0f;

    circleParams.maxEdgeLength =
        m_app
            ->pixelsToUnits(
                5.0f);

    gen->startShape();

    params.x0 =
        top_x
        + dx;

    params.y0 =
        top_y
        + dy;

    params.x1 =
        bottom_x
        + dx;

    params.y1 =
        bottom_y
        + dy;

    gen->generateLine2d(
        params);

    circleParams.center_x =
        params.x1;

    circleParams.center_y =
        params.y1;

    gen->generateCircle2d(
        circleParams);

    params.x0 =
        top_x
        - dx;

    params.y0 =
        top_y
        - dy;

    params.x1 =
        bottom_x
        - dx;

    params.y1 =
        bottom_y
        - dy;

    gen->generateLine2d(
        params);

    circleParams.center_x =
        params.x1;

    circleParams.center_y =
        params.y1;

    gen->generateCircle2d(
        circleParams);

    gen->endShape(
        &m_walls);
}

void CylinderBankObject::render(
    const ViewParameters *view)
{
    if (view->Sublayer != 0) {
        return;
    }

    /*
     * Cylinder banks used to be submitted at fixed layer 0.
     *
     * That happens to look acceptable for conventional inline
     * and V layouts because their banks are mostly separated
     * spatially.
     *
     * A W layout exposes the problem: several banks occupy
     * almost the same screen space, so fixed submission order
     * makes later-created banks paint over earlier ones
     * regardless of actual crank depth.
     *
     * Find the foremost currently-visible piston belonging to
     * this bank and use its mechanical layer to establish the
     * bank's render depth.
     */
    Piston *frontmostPiston =
        getForemostPiston(
            m_bank,
            view->Layer0);

    if (frontmostPiston == nullptr) {
        return;
    }

#if defined(ENGINE_SIM_IOS)
    const int visualLayer =
        frontmostPiston
            ->getCylinderIndex();

    const int renderLayer =
        0x31
        - visualLayer;
#else
    ConnectingRod *rod =
        frontmostPiston
            ->getRod();

    if (rod == nullptr) {
        return;
    }

    const int mechanicalLayer =
        rod->getLayer();

    const int renderLayer =
        0x31
        - mechanicalLayer;
#endif

    resetShader();

    const ysVector col =
        ysMath::Add(
            ysMath::Mul(
                m_app
                    ->getForegroundColor(),
                ysMath::LoadScalar(
                    0.01f)),
            ysMath::Mul(
                m_app
                    ->getBackgroundColor(),
                ysMath::LoadScalar(
                    0.99f)));

    /*
     * Preserve the current port's cylinder-bank color.
     *
     * "col" remains calculated because it is part of the
     * original implementation and may be useful when the
     * original muted bank styling is restored.
     */
    (void)col;

    m_app
        ->getShaders()
        ->SetBaseColor(
            m_app->getPink());

    m_app->drawGenerated(
        m_walls,
        renderLayer);
}

void CylinderBankObject::process(float dt) {
    /* void */
}

void CylinderBankObject::destroy() {
    /* void */
}
