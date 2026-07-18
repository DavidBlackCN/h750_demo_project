#include "PHASE_BLL.h"

#include "ADC_FML.h"
#include "FFT_FML.h"
#include "arm_math.h"
#include <math.h>

#define PHASE_PI                         3.14159265358979323846f
#define PHASE_MIN_FREQUENCY_HZ        1000.0f
#define PHASE_MAX_FREQUENCY_HZ      100000.0f
#define PHASE_SEARCH_MIN_HZ            500.0f
#define PHASE_SEARCH_MAX_HZ         120000.0f
#define PHASE_MIN_INPUT_VPP               0.05f
#define PHASE_MIN_FIT_QUALITY              0.98f
#define PHASE_GOLDEN_ITERATIONS              18U
#define PHASE_TRIANGLE_MIN_H3_RATIO         0.04f
#define PHASE_TRIANGLE_MAX_H3_RATE          0.45f
#define PHASE_TRIANGLE_MIN_FIT_QUALITY       0.94f
#define PHASE_TRIANGLE_MIN_CORRELATION       0.98f
#define PHASE_LAG_GOLDEN_ITERATIONS            14U

_Static_assert(ADC1_FREQ_BLOCK_LENGTH == FFT_LENGTH,
               "Dual ADC frame length must match the FFT length");

typedef struct
{
    float coefficient_cos;
    float coefficient_sin;
    float explained_power;
    float quality;
} phase_fit_t;

typedef struct
{
    float frequency_hz;
    float correction_deg;
} phase_calibration_point_t;

typedef struct
{
    float phase_deg;
    float correlation;
    uint8_t valid;
} phase_delay_result_t;

/*
 * Replace the zero corrections with same-source measurements from the actual
 * input network.  Linear interpolation is used between calibration points.
 */
static const phase_calibration_point_t s_phase_calibration[] =
{
    {  1000.0f, 0.0f},
    { 10000.0f, 0.0f},
    { 50000.0f, 0.0f},
    {100000.0f, 0.0f}
};

phase_result_t phase_result;

static float phase_wrap_deg(float phase_deg)
{
    while (phase_deg > 180.0f)
    {
        phase_deg -= 360.0f;
    }
    while (phase_deg <= -180.0f)
    {
        phase_deg += 360.0f;
    }
    return phase_deg;
}

float Phase_BLL_NominalFrequency(float measured_frequency_hz)
{
    return floorf(measured_frequency_hz / 1000.0f + 0.5f) * 1000.0f;
}

static float phase_calibration_deg(float frequency_hz)
{
    uint32_t count = sizeof(s_phase_calibration) / sizeof(s_phase_calibration[0]);

    if (frequency_hz <= s_phase_calibration[0].frequency_hz)
    {
        return s_phase_calibration[0].correction_deg;
    }

    for (uint32_t i = 1U; i < count; i++)
    {
        if (frequency_hz <= s_phase_calibration[i].frequency_hz)
        {
            float f0 = s_phase_calibration[i - 1U].frequency_hz;
            float f1 = s_phase_calibration[i].frequency_hz;
            float ratio = (frequency_hz - f0) / (f1 - f0);
            return s_phase_calibration[i - 1U].correction_deg +
                   ratio * (s_phase_calibration[i].correction_deg -
                            s_phase_calibration[i - 1U].correction_deg);
        }
    }

    return s_phase_calibration[count - 1U].correction_deg;
}

static phase_fit_t phase_fit_frequency(const float *channel1,
                                       const float *channel2,
                                       uint32_t length,
                                       float sample_rate_hz,
                                       float frequency_hz,
                                       phase_fit_t *channel2_fit)
{
    phase_fit_t fit1 = {0};
    phase_fit_t fit2 = {0};
    float omega = 2.0f * PHASE_PI * frequency_hz / sample_rate_hz;
    float step_cos = cosf(omega);
    float step_sin = sinf(omega);
    float oscillator_cos = 1.0f;
    float oscillator_sin = 0.0f;
    float sum_cos = 0.0f;
    float sum_sin = 0.0f;
    float sum_cos2 = 0.0f;
    float sum_sin2 = 0.0f;
    float sum_cos_sin = 0.0f;
    float sum_x1_cos = 0.0f;
    float sum_x1_sin = 0.0f;
    float sum_x2_cos = 0.0f;
    float sum_x2_sin = 0.0f;
    float power1 = 0.0f;
    float power2 = 0.0f;

    for (uint32_t i = 0U; i < length; i++)
    {
        float old_cos;

        sum_cos += oscillator_cos;
        sum_sin += oscillator_sin;
        sum_cos2 += oscillator_cos * oscillator_cos;
        sum_sin2 += oscillator_sin * oscillator_sin;
        sum_cos_sin += oscillator_cos * oscillator_sin;
        sum_x1_cos += channel1[i] * oscillator_cos;
        sum_x1_sin += channel1[i] * oscillator_sin;
        sum_x2_cos += channel2[i] * oscillator_cos;
        sum_x2_sin += channel2[i] * oscillator_sin;
        power1 += channel1[i] * channel1[i];
        power2 += channel2[i] * channel2[i];

        old_cos = oscillator_cos;
        oscillator_cos = old_cos * step_cos - oscillator_sin * step_sin;
        oscillator_sin = oscillator_sin * step_cos + old_cos * step_sin;

        if ((i & 0xFFU) == 0xFFU)
        {
            float norm = sqrtf(oscillator_cos * oscillator_cos +
                               oscillator_sin * oscillator_sin);
            if (norm > 0.0f)
            {
                oscillator_cos /= norm;
                oscillator_sin /= norm;
            }
        }
    }

    /* Center sine and cosine so the fit also includes an independent DC term. */
    sum_cos2 -= sum_cos * sum_cos / (float)length;
    sum_sin2 -= sum_sin * sum_sin / (float)length;
    sum_cos_sin -= sum_cos * sum_sin / (float)length;

    {
        float determinant = sum_cos2 * sum_sin2 - sum_cos_sin * sum_cos_sin;
        if (fabsf(determinant) > 1.0e-12f)
        {
            fit1.coefficient_cos = (sum_x1_cos * sum_sin2 -
                                    sum_x1_sin * sum_cos_sin) / determinant;
            fit1.coefficient_sin = (sum_x1_sin * sum_cos2 -
                                    sum_x1_cos * sum_cos_sin) / determinant;
            fit2.coefficient_cos = (sum_x2_cos * sum_sin2 -
                                    sum_x2_sin * sum_cos_sin) / determinant;
            fit2.coefficient_sin = (sum_x2_sin * sum_cos2 -
                                    sum_x2_cos * sum_cos_sin) / determinant;
        }
    }

    fit1.explained_power = fit1.coefficient_cos * sum_x1_cos +
                           fit1.coefficient_sin * sum_x1_sin;
    fit2.explained_power = fit2.coefficient_cos * sum_x2_cos +
                           fit2.coefficient_sin * sum_x2_sin;
    fit1.quality = (power1 > 0.0f) ? fit1.explained_power / power1 : 0.0f;
    fit2.quality = (power2 > 0.0f) ? fit2.explained_power / power2 : 0.0f;

    if (channel2_fit != NULL)
    {
        *channel2_fit = fit2;
    }
    return fit1;
}

static float phase_fit_amplitude(const phase_fit_t *fit)
{
    return sqrtf(fit->coefficient_cos * fit->coefficient_cos +
                 fit->coefficient_sin * fit->coefficient_sin);
}

static float phase_correlation_at_lag(const float *channel1,
                                      const float *channel2,
                                      uint32_t length, float lag_samples)
{
    int32_t lag_floor = (int32_t)floorf(lag_samples);
    float fraction = lag_samples - (float)lag_floor;
    uint32_t start = (lag_floor < 0) ? (uint32_t)(-lag_floor) : 0U;
    uint32_t end = (lag_floor >= 0) ?
                   (length - 1U - (uint32_t)lag_floor) : length;
    float sum1 = 0.0f;
    float sum2 = 0.0f;
    float sum11 = 0.0f;
    float sum22 = 0.0f;
    float sum12 = 0.0f;
    uint32_t count = 0U;

    if ((start >= end) || (end > length))
    {
        return -2.0f;
    }

    for (uint32_t i = start; i < end; i++)
    {
        int32_t index2 = (int32_t)i + lag_floor;
        float value1 = channel1[i];
        float value2 = channel2[index2] * (1.0f - fraction) +
                       channel2[index2 + 1] * fraction;

        sum1 += value1;
        sum2 += value2;
        sum11 += value1 * value1;
        sum22 += value2 * value2;
        sum12 += value1 * value2;
        count++;
    }

    if (count < 3U)
    {
        return -2.0f;
    }

    {
        float count_f = (float)count;
        float covariance = sum12 - sum1 * sum2 / count_f;
        float power1 = sum11 - sum1 * sum1 / count_f;
        float power2 = sum22 - sum2 * sum2 / count_f;
        float denominator = sqrtf(power1 * power2);

        return (denominator > 1.0e-12f) ? covariance / denominator : -2.0f;
    }
}

static phase_delay_result_t phase_triangle_delay(const float *channel1,
                                                 const float *channel2,
                                                 uint32_t length,
                                                 float sample_rate_hz,
                                                 float frequency_hz,
                                                 float phase_hint_deg)
{
    const float golden_ratio = 0.61803398875f;
    phase_delay_result_t result = {0};
    float samples_per_period = sample_rate_hz / frequency_hz;
    float expected_lag = -phase_hint_deg * samples_per_period / 360.0f;
    float minimum_lag_f = expected_lag - 0.25f * samples_per_period;
    float maximum_lag_f = expected_lag + 0.25f * samples_per_period;
    int32_t minimum_lag;
    int32_t maximum_lag;
    int32_t best_integer_lag = 0;
    float best_correlation = -2.0f;
    float left;
    float right;
    float x1;
    float x2;
    float y1;
    float y2;
    float best_lag;

    if (minimum_lag_f < -0.5f * samples_per_period)
    {
        minimum_lag_f = -0.5f * samples_per_period;
    }
    if (maximum_lag_f > 0.5f * samples_per_period)
    {
        maximum_lag_f = 0.5f * samples_per_period;
    }
    minimum_lag = (int32_t)ceilf(minimum_lag_f);
    maximum_lag = (int32_t)floorf(maximum_lag_f);

    if ((samples_per_period < 4.0f) || (maximum_lag < minimum_lag))
    {
        return result;
    }

    for (int32_t lag = minimum_lag; lag <= maximum_lag; lag++)
    {
        float correlation = phase_correlation_at_lag(
            channel1, channel2, length, (float)lag);
        if (correlation > best_correlation)
        {
            best_correlation = correlation;
            best_integer_lag = lag;
        }
    }

    left = (float)best_integer_lag - 1.0f;
    right = (float)best_integer_lag + 1.0f;
    if (left < minimum_lag_f) left = minimum_lag_f;
    if (right > maximum_lag_f) right = maximum_lag_f;

    x1 = right - golden_ratio * (right - left);
    x2 = left + golden_ratio * (right - left);
    y1 = phase_correlation_at_lag(channel1, channel2, length, x1);
    y2 = phase_correlation_at_lag(channel1, channel2, length, x2);

    for (uint32_t i = 0U; i < PHASE_LAG_GOLDEN_ITERATIONS; i++)
    {
        if (y1 < y2)
        {
            left = x1;
            x1 = x2;
            y1 = y2;
            x2 = left + golden_ratio * (right - left);
            y2 = phase_correlation_at_lag(channel1, channel2, length, x2);
        }
        else
        {
            right = x2;
            x2 = x1;
            y2 = y1;
            x1 = right - golden_ratio * (right - left);
            y1 = phase_correlation_at_lag(channel1, channel2, length, x1);
        }
    }

    best_lag = 0.5f * (left + right);
    result.correlation = phase_correlation_at_lag(channel1, channel2,
                                                  length, best_lag);
    result.phase_deg = phase_wrap_deg(
        -best_lag * 360.0f / samples_per_period);
    result.valid = (result.correlation >= PHASE_TRIANGLE_MIN_CORRELATION) ? 1U : 0U;
    return result;
}

static float phase_fit_objective(const float *channel1,
                                 const float *channel2,
                                 uint32_t length,
                                 float sample_rate_hz,
                                 float frequency_hz)
{
    phase_fit_t fit2;
    phase_fit_t fit1 = phase_fit_frequency(channel1, channel2, length,
                                           sample_rate_hz,
                                           frequency_hz, &fit2);
    return fit1.explained_power + fit2.explained_power;
}

static float phase_refine_frequency(const float *channel1,
                                    const float *channel2,
                                    uint32_t length,
                                    float sample_rate_hz,
                                    float coarse_frequency_hz)
{
    const float golden_ratio = 0.61803398875f;
    const float bin_width = sample_rate_hz / (float)FFT_LENGTH;
    float left;
    float right;
    float x1;
    float x2;
    float y1;
    float y2;

    if (coarse_frequency_hz < PHASE_SEARCH_MIN_HZ)
    {
        coarse_frequency_hz = PHASE_SEARCH_MIN_HZ;
    }
    else if (coarse_frequency_hz > PHASE_SEARCH_MAX_HZ)
    {
        coarse_frequency_hz = PHASE_SEARCH_MAX_HZ;
    }

    left = coarse_frequency_hz - bin_width;
    right = coarse_frequency_hz + bin_width;
    if (left < PHASE_SEARCH_MIN_HZ)
    {
        left = PHASE_SEARCH_MIN_HZ;
    }
    if (right > PHASE_SEARCH_MAX_HZ)
    {
        right = PHASE_SEARCH_MAX_HZ;
    }

    x1 = right - golden_ratio * (right - left);
    x2 = left + golden_ratio * (right - left);
    y1 = phase_fit_objective(channel1, channel2, length, sample_rate_hz, x1);
    y2 = phase_fit_objective(channel1, channel2, length, sample_rate_hz, x2);

    for (uint32_t i = 0U; i < PHASE_GOLDEN_ITERATIONS; i++)
    {
        if (y1 < y2)
        {
            left = x1;
            x1 = x2;
            y1 = y2;
            x2 = left + golden_ratio * (right - left);
            y2 = phase_fit_objective(channel1, channel2, length, sample_rate_hz, x2);
        }
        else
        {
            right = x2;
            x2 = x1;
            y2 = y1;
            x1 = right - golden_ratio * (right - left);
            y1 = phase_fit_objective(channel1, channel2, length, sample_rate_hz, x1);
        }
    }

    return 0.5f * (left + right);
}

static uint8_t phase_center_inputs(float *channel1, float *channel2,
                                   uint32_t length)
{
    float mean1 = 0.0f;
    float mean2 = 0.0f;
    float minimum1;
    float maximum1;
    float minimum2;
    float maximum2;

    if ((channel1 == NULL) || (channel2 == NULL) || (length == 0U))
    {
        return 0U;
    }

    minimum1 = channel1[0];
    maximum1 = channel1[0];
    minimum2 = channel2[0];
    maximum2 = channel2[0];

    for (uint32_t i = 0U; i < length; i++)
    {
        mean1 += channel1[i];
        mean2 += channel2[i];
        if (channel1[i] < minimum1) minimum1 = channel1[i];
        if (channel1[i] > maximum1) maximum1 = channel1[i];
        if (channel2[i] < minimum2) minimum2 = channel2[i];
        if (channel2[i] > maximum2) maximum2 = channel2[i];
    }
    mean1 /= (float)length;
    mean2 /= (float)length;
    phase_result.channel1_dc = mean1;
    phase_result.channel2_dc = mean2;
    phase_result.channel1_raw_vpp = maximum1 - minimum1;
    phase_result.channel2_raw_vpp = maximum2 - minimum2;

    for (uint32_t i = 0U; i < length; i++)
    {
        channel1[i] -= mean1;
        channel2[i] -= mean2;
    }
    return 1U;
}

float Phase_BLL_EstimateFrequency(float *channel1, float *channel2,
                                  uint32_t length, float sample_rate_hz)
{
    float coarse_frequency_hz;

    if ((length != FFT_LENGTH) || (sample_rate_hz <= 0.0f) ||
        !phase_center_inputs(channel1, channel2, length))
    {
        return 0.0f;
    }

    calculate_fft(channel1, length);
    {
        uint32_t peak_index = get_fft_max_index(fft_out, FFT_LENGTH);
        float peak_bin = (float)peak_index;

        if ((peak_index > DC_INDEX) && (peak_index + 1U < FFT_LENGTH / 2U))
        {
            float y0 = fft_out[peak_index - 1U];
            float y1 = fft_out[peak_index];
            float y2 = fft_out[peak_index + 1U];
            float denominator = y0 - 2.0f * y1 + y2;
            if (fabsf(denominator) > 1.0e-12f)
            {
                float delta = 0.5f * (y0 - y2) / denominator;
                if (delta > 0.5f) delta = 0.5f;
                if (delta < -0.5f) delta = -0.5f;
                peak_bin += delta;
            }
        }
        coarse_frequency_hz = peak_bin * sample_rate_hz /
                              (float)FFT_LENGTH;
    }

    return phase_refine_frequency(channel1, channel2, length,
                                  sample_rate_hz, coarse_frequency_hz);
}

void Phase_BLL_ProcessKnownFrequency(float *channel1, float *channel2,
                                     uint32_t length, float sample_rate_hz,
                                     float frequency_hz)
{
    phase_fit_t fit1;
    phase_fit_t fit2;
    phase_fit_t harmonic3_fit1 = {0};
    phase_fit_t harmonic3_fit2 = {0};
    phase_delay_result_t delay_result = {0};
    float phase1_deg;
    float phase2_deg;
    float amplitude1;
    float amplitude2;
    uint8_t triangle_wave = 0U;
    uint8_t fit_quality_valid;

    phase_result.valid = 0U;
    /* Inputs are known to be integer-kHz tones.  Keep frequency_hz for the
       discrete-time fit, but report and calibrate against the nominal tone. */
    phase_result.frequency_hz = Phase_BLL_NominalFrequency(frequency_hz);
    if ((sample_rate_hz <= 0.0f) || (frequency_hz <= 0.0f) ||
        !phase_center_inputs(channel1, channel2, length))
    {
        return;
    }

    fit1 = phase_fit_frequency(channel1, channel2, length,
                               sample_rate_hz, frequency_hz, &fit2);

    amplitude1 = phase_fit_amplitude(&fit1);
    amplitude2 = phase_fit_amplitude(&fit2);
    phase1_deg = atan2f(-fit1.coefficient_sin, fit1.coefficient_cos) *
                 180.0f / PHASE_PI;
    phase2_deg = atan2f(-fit2.coefficient_sin, fit2.coefficient_cos) *
                 180.0f / PHASE_PI;
    if ((3.0f * frequency_hz) <=
        (PHASE_TRIANGLE_MAX_H3_RATE * sample_rate_hz))
    {
        harmonic3_fit1 = phase_fit_frequency(channel1, channel2, length,
                                             sample_rate_hz,
                                             3.0f * frequency_hz,
                                             &harmonic3_fit2);
        if ((amplitude1 > 1.0e-6f) && (amplitude2 > 1.0e-6f) &&
            ((phase_fit_amplitude(&harmonic3_fit1) / amplitude1) >=
             PHASE_TRIANGLE_MIN_H3_RATIO) &&
            ((phase_fit_amplitude(&harmonic3_fit2) / amplitude2) >=
             PHASE_TRIANGLE_MIN_H3_RATIO))
        {
            triangle_wave = 1U;
            delay_result = phase_triangle_delay(channel1, channel2, length,
                                                sample_rate_hz, frequency_hz,
                                                phase_wrap_deg(phase2_deg -
                                                               phase1_deg));
        }
    }

    phase_result.phase_difference_deg = phase_wrap_deg(
        ((triangle_wave && delay_result.valid) ? delay_result.phase_deg :
         (phase2_deg - phase1_deg)) -
        phase_calibration_deg(phase_result.frequency_hz));
    phase_result.channel1_vpp = 2.0f * amplitude1;
    phase_result.channel2_vpp = 2.0f * amplitude2;
    phase_result.channel1_fit_quality = fit1.quality;
    phase_result.channel2_fit_quality = fit2.quality;
    fit_quality_valid = triangle_wave ?
        ((fit1.quality >= PHASE_TRIANGLE_MIN_FIT_QUALITY) &&
         (fit2.quality >= PHASE_TRIANGLE_MIN_FIT_QUALITY)) :
        ((fit1.quality >= PHASE_MIN_FIT_QUALITY) &&
         (fit2.quality >= PHASE_MIN_FIT_QUALITY));

    if ((phase_result.frequency_hz >= PHASE_MIN_FREQUENCY_HZ) &&
        (phase_result.frequency_hz <= PHASE_MAX_FREQUENCY_HZ) &&
        (phase_result.channel1_vpp >= PHASE_MIN_INPUT_VPP) &&
        (phase_result.channel2_vpp >= PHASE_MIN_INPUT_VPP) &&
        fit_quality_valid &&
        (!triangle_wave || delay_result.valid))
    {
        phase_result.valid = 1U;
    }
}

void Phase_BLL_Process(float *channel1, float *channel2, uint32_t length)
{
    float frequency_hz = Phase_BLL_EstimateFrequency(channel1, channel2,
                                                      length,
                                                      ADC_DUAL_SAMPLE_RATE_HZ);
    Phase_BLL_ProcessKnownFrequency(channel1, channel2, length,
                                    ADC_DUAL_SAMPLE_RATE_HZ, frequency_hz);
}
