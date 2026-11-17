#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "application_registers.h"
#include "control_application.h"

static void initialize_healthy_system(void)
{
    ApplicationRegisters_Initialize();
    ControlApplication_Initialize();
    ApplicationRegisters_SetInternal(
        APP_REG_SYSTEM_STATUS,
        APP_STATUS_IDLE | APP_STATUS_SENSOR_CARD_OK | APP_STATUS_CONTROLLER_OK);
    ApplicationRegisters_SetInternal(APP_REG_FAULT_FLAGS, 0U);
    ApplicationRegisters_SetInternal(APP_REG_CHAMBER_TEMPERATURE, 250U);
    ApplicationRegisters_SetInternal(APP_REG_HEATER_TEMPERATURE, 250U);
    ApplicationRegisters_SetInternal(APP_REG_EVAP_BOX_TEMPERATURE, 250U);
    ApplicationRegisters_SetInternal(APP_REG_SUPERHEAT, 100U);
}

static void test_manual_outputs(void)
{
    initialize_healthy_system();
    ApplicationRegisters_SetInternal(APP_REG_MANUAL_HEATER_PWM, 250U);
    ApplicationRegisters_SetInternal(APP_REG_MANUAL_EEV_PWM, 20U);
    assert(ApplicationRegisters_WriteCoil(122U, true) == APP_WRITE_OK);
    assert(ApplicationRegisters_WriteCoil(123U, true) == APP_WRITE_OK);
    assert(ApplicationRegisters_WriteCoil(112U, true) == APP_WRITE_OK);

    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_HEATER_PWM) == 250U);
    assert(ApplicationRegisters_Get(APP_REG_EEV_PWM) == 20U);
    assert(ApplicationRegisters_Get(APP_REG_EEV_STATE) ==
           APP_EEV_STATE_MANUAL);
    assert(ApplicationRegisters_Get(APP_REG_SYSTEM_STATE) == APP_STATE_MANUAL);
    assert(ApplicationRegisters_Get(APP_REG_COMPRESSOR_STATE) == 1U);
    assert((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) &
            (APP_ACT_HEATER_PWM_ACTIVE | APP_ACT_AC_COMPRESSOR)) ==
           (APP_ACT_HEATER_PWM_ACTIVE | APP_ACT_AC_COMPRESSOR));
}

static void test_automatic_heating(void)
{
    initialize_healthy_system();
    ApplicationRegisters_SetInternal(APP_REG_SYSTEM_MODE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_RAMP_RATE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_MAIN_SETPOINT, 300U);
    ApplicationRegisters_SetInternal(APP_REG_CHAMBER_TEMPERATURE, 200U);

    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_SYSTEM_STATE) ==
           APP_STATE_HEATING);
    assert(ApplicationRegisters_Get(APP_REG_HEATING_ACTIVE) == 1U);
    assert(ApplicationRegisters_Get(APP_REG_HEATER_PWM) > 0U);
    assert((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) &
            APP_ACT_HEAT_FANS) != 0U);
    assert(ApplicationRegisters_Get(APP_REG_COMPRESSOR_STATE) == 0U);
}

static void test_automatic_refrigeration(void)
{
    initialize_healthy_system();
    ApplicationRegisters_SetInternal(APP_REG_SYSTEM_MODE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_RAMP_RATE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_MAIN_SETPOINT, 200U);
    ApplicationRegisters_SetInternal(APP_REG_CHAMBER_TEMPERATURE, 300U);
    ApplicationRegisters_SetInternal(APP_REG_EVAP_BOX_TEMPERATURE, 295U);
    ApplicationRegisters_SetInternal(APP_REG_COMP_MIN_OFF_TIME, 0U);
    ApplicationRegisters_SetInternal(APP_REG_COMP_MIN_ON_TIME, 0U);

    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_COMPRESSOR_STATE) == 1U);
    assert(ApplicationRegisters_Get(APP_REG_EEV_STATE) ==
           APP_EEV_STATE_STARTUP);
    assert(ApplicationRegisters_Get(APP_REG_EEV_PWM) == 0U);

    for (uint8_t second = 0U; second < 7U; second++) {
        ControlApplication_Step1s();
    }
    assert(ApplicationRegisters_Get(APP_REG_EEV_STATE) ==
           APP_EEV_STATE_CONTROL);
    assert(ApplicationRegisters_Get(APP_REG_EEV_PWM) >=
           ApplicationRegisters_Get(APP_REG_EEV_MIN_PWM));
    assert((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) &
            APP_ACT_AC_COMPRESSOR) != 0U);
}

static void test_free_cooling(void)
{
    initialize_healthy_system();
    ApplicationRegisters_SetInternal(APP_REG_SYSTEM_MODE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_RAMP_RATE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_MAIN_SETPOINT, 200U);
    ApplicationRegisters_SetInternal(APP_REG_CHAMBER_TEMPERATURE, 300U);
    ApplicationRegisters_SetInternal(APP_REG_EVAP_BOX_TEMPERATURE, 250U);

    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_SYSTEM_STATE) ==
           APP_STATE_FREE_COOLING);
    assert(ApplicationRegisters_Get(APP_REG_FREE_COOL_ACTIVE) == 1U);
    assert(ApplicationRegisters_Get(APP_REG_COMPRESSOR_STATE) == 0U);
    assert((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) &
            APP_ACT_COOL_FANS) != 0U);
}

static void start_refrigeration_for_reheat(void)
{
    initialize_healthy_system();
    ApplicationRegisters_SetInternal(APP_REG_SYSTEM_MODE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_RAMP_RATE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_MAIN_SETPOINT, 250U);
    ApplicationRegisters_SetInternal(APP_REG_CHAMBER_TEMPERATURE, 300U);
    ApplicationRegisters_SetInternal(APP_REG_EVAP_BOX_TEMPERATURE, 295U);
    ApplicationRegisters_SetInternal(APP_REG_COMP_MIN_OFF_TIME, 0U);
    ApplicationRegisters_SetInternal(APP_REG_COMP_MIN_ON_TIME, 10U);
    /* Isolate output selection from integral history and derivative response. */
    ApplicationRegisters_SetInternal(APP_REG_PID_KI, 0U);
    ApplicationRegisters_SetInternal(APP_REG_PID_KD, 0U);
    for (uint8_t second = 0U; second < 8U; second++) {
        ControlApplication_Step1s();
    }
    assert(ApplicationRegisters_Get(APP_REG_COMPRESSOR_STATE) == 1U);
    assert(ApplicationRegisters_Get(APP_REG_EEV_STATE) == APP_EEV_STATE_CONTROL);
    ApplicationRegisters_SetInternal(APP_REG_CHAMBER_TEMPERATURE, 240U);
}

static void test_automatic_reheat_and_compressor_timing(void)
{
    start_refrigeration_for_reheat();
    for (uint8_t second = 8U; second < 10U; second++) {
        ControlApplication_Step1s();
        uint16_t concurrent = APP_ACT_AC_COMPRESSOR |
            APP_ACT_AC_CONDENSER_FAN | APP_ACT_HEATER_PWM_ACTIVE |
            APP_ACT_HEAT_FANS | APP_ACT_COOL_FANS | APP_ACT_EEV_PWM_ACTIVE;
        assert((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) &
                concurrent) == concurrent);
        assert(ApplicationRegisters_Get(APP_REG_HEATER_PWM) > 0U);
        assert(ApplicationRegisters_Get(APP_REG_COMPRESSOR_STATE) == 1U);
        assert(ApplicationRegisters_Get(APP_REG_EEV_STATE) ==
               APP_EEV_STATE_CONTROL);
        assert(ApplicationRegisters_Get(APP_REG_SYSTEM_STATE) == APP_STATE_HEATING);
        assert(ApplicationRegisters_Get(APP_REG_HEATING_ACTIVE) == 1U);
        assert(ApplicationRegisters_Get(APP_REG_COOLING_ACTIVE) == 0U);
    }
    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_COMPRESSOR_STATE) == 0U);
    assert(ApplicationRegisters_Get(APP_REG_HEATER_PWM) > 0U);
    assert(ApplicationRegisters_Get(APP_REG_EEV_PWM) == 0U);
    assert((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) &
            (APP_ACT_AC_COMPRESSOR | APP_ACT_AC_CONDENSER_FAN |
             APP_ACT_COOL_FANS | APP_ACT_EEV_PWM_ACTIVE)) == 0U);

    ApplicationRegisters_SetInternal(APP_REG_COMP_MIN_OFF_TIME, 2U);
    ApplicationRegisters_SetInternal(APP_REG_CHAMBER_TEMPERATURE, 300U);
    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_COMPRESSOR_STATE) == 0U);
    assert(ApplicationRegisters_Get(APP_REG_HEATER_PWM) == 0U);
    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_COMPRESSOR_STATE) == 1U);
}

static void test_cooling_only_disables_reheat(void)
{
    start_refrigeration_for_reheat();
    ApplicationRegisters_SetInternal(APP_REG_OPERATING_MODE, 2U);
    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_COMPRESSOR_STATE) == 1U);
    assert(ApplicationRegisters_Get(APP_REG_HEATER_PWM) == 0U);
    assert((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) &
            (APP_ACT_HEATER_PWM_ACTIVE | APP_ACT_HEAT_FANS)) == 0U);
}

static void test_protection_during_concurrent_operation(void)
{
    const uint16_t causes[] = {
        APP_SAFETY_HEATER, APP_SAFETY_SENSOR_COMMUNICATION,
        APP_SAFETY_EVAPORATOR_LIMIT, APP_SAFETY_CHAMBER_LIMIT,
        APP_SAFETY_PI_COMMUNICATION
    };
    for (uint8_t index = 0U; index < sizeof(causes) / sizeof(causes[0]); index++) {
        start_refrigeration_for_reheat();
        ControlApplication_Step1s();
        assert(ApplicationRegisters_Get(APP_REG_HEATER_PWM) > 0U);
        assert(ApplicationRegisters_Get(APP_REG_COMPRESSOR_STATE) == 1U);
        switch (causes[index]) {
            case APP_SAFETY_HEATER:
                ApplicationRegisters_SetInternal(APP_REG_HEATER_TEMPERATURE, 1400U);
                break;
            case APP_SAFETY_SENSOR_COMMUNICATION:
                ApplicationRegisters_SetInternal(APP_REG_FAULT_FLAGS,
                    APP_FAULT_SENSOR_COMMUNICATION);
                break;
            case APP_SAFETY_EVAPORATOR_LIMIT:
                ApplicationRegisters_SetInternal(APP_REG_EVAP_BOX_TEMPERATURE,
                    (uint16_t)(int16_t)-350);
                break;
            case APP_SAFETY_CHAMBER_LIMIT:
                ApplicationRegisters_SetInternal(APP_REG_CHAMBER_TEMPERATURE, 650U);
                break;
            case APP_SAFETY_PI_COMMUNICATION:
                assert(ControlApplication_CheckPiTimeout(CONTROL_PI_TIMEOUT_MS));
                break;
        }
        ControlApplication_Step1s();
        assert(ApplicationRegisters_Get(APP_REG_SAFETY_CODE) == causes[index]);
        assert(ApplicationRegisters_Get(APP_REG_SAFETY_LATCHED) == 1U);
        assert(ApplicationRegisters_Get(APP_REG_HEATER_PWM) == 0U);
        assert(ApplicationRegisters_Get(APP_REG_EEV_PWM) == 0U);
        assert(ApplicationRegisters_Get(APP_REG_COMPRESSOR_STATE) == 0U);
        assert(ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) == APP_ACT_RED_PILOT);
    }
}

static void test_latched_heater_safety(void)
{
    initialize_healthy_system();
    ApplicationRegisters_SetInternal(APP_REG_MANUAL_HEATER_PWM, 500U);
    assert(ApplicationRegisters_WriteCoil(122U, true) == APP_WRITE_OK);
    ApplicationRegisters_SetInternal(APP_REG_HEATER_TEMPERATURE, 1400U);

    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_ACTIVE) == 1U);
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_LATCHED) == 1U);
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_CODE) ==
           APP_SAFETY_HEATER);
    assert(ApplicationRegisters_Get(APP_REG_HEATER_PWM) == 0U);
    assert((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) &
            APP_ACT_RED_PILOT) != 0U);

    ApplicationRegisters_SetInternal(APP_REG_HEATER_TEMPERATURE, 250U);
    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_ACTIVE) == 1U);
    ApplicationRegisters_SetInternal(APP_REG_SAFETY_LATCHED, 0U);
    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_ACTIVE) == 0U);
}

static void test_manual_compressor_evaporator_limit(void)
{
    initialize_healthy_system();
    ApplicationRegisters_SetInternal(APP_REG_EVAP_MIN_TEMPERATURE,
                                     (uint16_t)(int16_t)-100);
    ApplicationRegisters_SetInternal(APP_REG_EVAP_BOX_TEMPERATURE,
                                     (uint16_t)(int16_t)-120);
    assert(ApplicationRegisters_WriteCoil(112U, true) == APP_WRITE_OK);

    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_ACTIVE) == 1U);
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_CODE) ==
           APP_SAFETY_EVAPORATOR_LIMIT);
    assert((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) &
            APP_ACT_AC_COMPRESSOR) == 0U);
}

static void test_pi_deadline_and_regular_polling(void)
{
    initialize_healthy_system();
    assert(!ControlApplication_CheckPiTimeout(CONTROL_PI_TIMEOUT_MS - 1U));
    assert(ControlApplication_CheckPiTimeout(CONTROL_PI_TIMEOUT_MS));
    assert(!ControlApplication_CheckPiTimeout(CONTROL_PI_TIMEOUT_MS + 1U));

    initialize_healthy_system();
    for (uint32_t now = 0U; now <= 60000U; now += 3000U) {
        assert(!ControlApplication_CheckPiTimeout(now));
        ControlApplication_OnPiRequest(now);
        ControlApplication_Step1s();
        assert(ApplicationRegisters_Get(APP_REG_SAFETY_ACTIVE) == 0U);
    }
    assert(!ControlApplication_CheckPiTimeout(74999U));
    assert(ControlApplication_CheckPiTimeout(75000U));
}

static void test_pi_trip_in_both_modes(void)
{
    for (uint16_t mode = 0U; mode <= 1U; mode++) {
        initialize_healthy_system();
        ApplicationRegisters_SetInternal(APP_REG_SYSTEM_MODE, mode);
        ApplicationRegisters_SetInternal(APP_REG_ACTUATOR_STATES,
            APP_ACT_AC_COMPRESSOR | APP_ACT_AC_CONDENSER_FAN |
            APP_ACT_AC_DEFROST | APP_ACT_DC_COMPRESSOR |
            APP_ACT_HEAT_FANS | APP_ACT_COOL_FANS |
            APP_ACT_HEATER_PWM_ACTIVE | APP_ACT_EEV_PWM_ACTIVE |
            APP_ACT_GREEN_PILOT | APP_ACT_ORANGE_PILOT);
        ApplicationRegisters_SetInternal(APP_REG_HEATER_PWM, 500U);
        ApplicationRegisters_SetInternal(APP_REG_EEV_PWM, 20U);
        ControlApplication_OnPiRequest(100U);
        assert(ControlApplication_CheckPiTimeout(15100U));
        assert(ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) ==
               APP_ACT_RED_PILOT);
        assert(ApplicationRegisters_Get(APP_REG_HEATER_PWM) == 0U);
        assert(ApplicationRegisters_Get(APP_REG_EEV_PWM) == 0U);
        assert(ApplicationRegisters_Get(APP_REG_SAFETY_LATCHED) == 1U);
        assert(ApplicationRegisters_Get(APP_REG_SAFETY_CODE) ==
               APP_SAFETY_PI_COMMUNICATION);
        ControlApplication_Step1s();
        assert(ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) ==
               APP_ACT_RED_PILOT);
    }
}

static void test_pi_reconnection_requires_reset(void)
{
    initialize_healthy_system();
    assert(ControlApplication_CheckPiTimeout(15000U));
    uint16_t reset = 0U;
    assert(ApplicationRegisters_Write(APP_REG_SAFETY_LATCHED, &reset, 1U) ==
           APP_WRITE_OK);
    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_LATCHED) == 1U);
    ControlApplication_OnPiRequest(16000U);
    assert((ApplicationRegisters_Get(APP_REG_FAULT_FLAGS) &
            APP_FAULT_PI_COMMUNICATION) == 0U);
    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_ACTIVE) == 1U);
    assert(ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) == APP_ACT_RED_PILOT);
    assert(ApplicationRegisters_Write(APP_REG_SAFETY_LATCHED, &reset, 1U) ==
           APP_WRITE_OK);
    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_ACTIVE) == 0U);
    assert(ApplicationRegisters_Get(APP_REG_HEATER_PWM) == 0U);
}

static void test_pi_clock_wrap_and_first_fault(void)
{
    initialize_healthy_system();
    uint32_t last = UINT32_MAX - 5000U;
    ControlApplication_OnPiRequest(last);
    assert(!ControlApplication_CheckPiTimeout(last + CONTROL_PI_TIMEOUT_MS - 1U));
    assert(ControlApplication_CheckPiTimeout(last + CONTROL_PI_TIMEOUT_MS));

    initialize_healthy_system();
    ApplicationRegisters_SetInternal(APP_REG_HEATER_TEMPERATURE, 1400U);
    ControlApplication_Step1s();
    assert(ControlApplication_CheckPiTimeout(15000U));
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_CODE) == APP_SAFETY_HEATER);
}

int main(void)
{
    test_manual_outputs();
    test_automatic_heating();
    test_automatic_refrigeration();
    test_free_cooling();
    test_automatic_reheat_and_compressor_timing();
    test_cooling_only_disables_reheat();
    test_protection_during_concurrent_operation();
    test_latched_heater_safety();
    test_manual_compressor_evaporator_limit();
    test_pi_deadline_and_regular_polling();
    test_pi_trip_in_both_modes();
    test_pi_reconnection_requires_reset();
    test_pi_clock_wrap_and_first_fault();
    puts("control application tests passed");
    return 0;
}
