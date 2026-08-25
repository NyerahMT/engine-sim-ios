#include "../include/convolution_filter.h"

#include <assert.h>
#include <string.h>

#if defined(__aarch64__) || defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace {

/*
 * Exact same dot product that the original scalar convolution performs,
 * but process multiple float samples per CPU instruction on ARM64.
 *
 * This does NOT shorten the impulse response and does NOT change the
 * convolution model. It only accelerates the arithmetic.
 */
inline float dotProductFast(
    const float *a,
    const float *b,
    int count)
{
    if (count <= 0) {
        return 0.0f;
    }

#if defined(__aarch64__)

    int i = 0;

    /*
     * Two accumulators reduce the dependency chain and let Apple's ARM
     * cores execute more FMAs in parallel.
     */
    float32x4_t accumulator0 =
        vdupq_n_f32(0.0f);

    float32x4_t accumulator1 =
        vdupq_n_f32(0.0f);

    for (
        ;
        i + 8 <= count;
        i += 8)
    {
        const float32x4_t a0 =
            vld1q_f32(
                a + i);

        const float32x4_t b0 =
            vld1q_f32(
                b + i);

        const float32x4_t a1 =
            vld1q_f32(
                a + i + 4);

        const float32x4_t b1 =
            vld1q_f32(
                b + i + 4);

        accumulator0 =
            vfmaq_f32(
                accumulator0,
                a0,
                b0);

        accumulator1 =
            vfmaq_f32(
                accumulator1,
                a1,
                b1);
    }

    accumulator0 =
        vaddq_f32(
            accumulator0,
            accumulator1);

    float result =
        vaddvq_f32(
            accumulator0);

    /*
     * Handle the final 0-7 samples normally.
     */
    for (
        ;
        i < count;
        ++i)
    {
        result +=
            a[i]
            * b[i];
    }

    return result;

#elif defined(__ARM_NEON)

    int i = 0;

    float32x4_t accumulator =
        vdupq_n_f32(0.0f);

    for (
        ;
        i + 4 <= count;
        i += 4)
    {
        accumulator =
            vmlaq_f32(
                accumulator,
                vld1q_f32(a + i),
                vld1q_f32(b + i));
    }

    float32x2_t sum =
        vadd_f32(
            vget_low_f32(accumulator),
            vget_high_f32(accumulator));

    sum =
        vpadd_f32(
            sum,
            sum);

    float result =
        vget_lane_f32(
            sum,
            0);

    for (
        ;
        i < count;
        ++i)
    {
        result +=
            a[i]
            * b[i];
    }

    return result;

#else

    /*
     * Desktop/simulator fallback.
     */
    float result =
        0.0f;

    for (
        int i = 0;
        i < count;
        ++i)
    {
        result +=
            a[i]
            * b[i];
    }

    return result;

#endif
}

}

ConvolutionFilter::ConvolutionFilter() {
    m_shiftRegister =
        nullptr;

    m_impulseResponse =
        nullptr;

    m_shiftOffset =
        0;

    m_sampleCount =
        0;
}

ConvolutionFilter::~ConvolutionFilter() {
    assert(
        m_shiftRegister
        == nullptr);

    assert(
        m_impulseResponse
        == nullptr);
}

void ConvolutionFilter::initialize(
    int samples)
{
    /*
     * Be defensive about empty impulse responses.
     */
    if (samples <= 0) {
        m_sampleCount =
            0;

        m_shiftOffset =
            0;

        m_shiftRegister =
            nullptr;

        m_impulseResponse =
            nullptr;

        return;
    }

    m_sampleCount =
        samples;

    m_shiftOffset =
        0;

    m_shiftRegister =
        new float[samples];

    m_impulseResponse =
        new float[samples];

    memset(
        m_shiftRegister,
        0,
        sizeof(float)
            * samples);

    memset(
        m_impulseResponse,
        0,
        sizeof(float)
            * samples);
}

void ConvolutionFilter::destroy() {
    if (
        m_shiftRegister
        != nullptr)
    {
        delete[]
            m_shiftRegister;
    }

    if (
        m_impulseResponse
        != nullptr)
    {
        delete[]
            m_impulseResponse;
    }

    m_shiftRegister =
        nullptr;

    m_impulseResponse =
        nullptr;

    m_shiftOffset =
        0;

    m_sampleCount =
        0;
}

float ConvolutionFilter::f(
    float sample)
{
    if (
        m_sampleCount <= 0
        || m_shiftRegister == nullptr
        || m_impulseResponse == nullptr)
    {
        return sample;
    }

    /*
     * Insert newest sample exactly where upstream EngineSim did.
     */
    m_shiftRegister[
        m_shiftOffset] =
        sample;

    /*
     * The circular history buffer is physically split into at most
     * two contiguous regions.
     *
     * Upstream evaluated both regions one scalar float at a time.
     *
     * We evaluate those exact same two regions as SIMD dot products.
     */

    const int firstCount =
        m_sampleCount
        - m_shiftOffset;

    const int secondCount =
        m_shiftOffset;

    float result =
        0.0f;

    if (
        firstCount > 0)
    {
        result +=
            dotProductFast(
                m_impulseResponse,
                m_shiftRegister
                    + m_shiftOffset,
                firstCount);
    }

    if (
        secondCount > 0)
    {
        result +=
            dotProductFast(
                m_impulseResponse
                    + firstCount,
                m_shiftRegister,
                secondCount);
    }

    /*
     * Preserve upstream circular-buffer ordering.
     */
    --m_shiftOffset;

    if (
        m_shiftOffset < 0)
    {
        m_shiftOffset =
            m_sampleCount - 1;
    }

    return result;
}
