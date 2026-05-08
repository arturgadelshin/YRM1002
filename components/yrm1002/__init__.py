from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_ON_TAG,
    CONF_ON_TAG_REMOVED,
    CONF_TRIGGER_ID,
    CONF_UART_ID,
)
from esphome.components import uart, binary_sensor

CODEOWNERS = ["@arturgadelshin"]
AUTO_LOAD = ["binary_sensor", "text_sensor"]
MULTI_CONF = True

CONF_YRM1002_ID = "yrm1002_id"
CONF_SCAN_DURATION = "scan_duration"
CONF_REPEAT_COUNT = "repeat_count"
CONF_TAG_PRESENT = "tag_present"

yrm1002_ns = cg.esphome_ns.namespace("yrm1002")
YRM1002 = yrm1002_ns.class_("YRM1002", cg.PollingComponent, uart.UARTDevice)
TagInfo = yrm1002_ns.struct("TagInfo")

YRM1002TagTrigger = yrm1002_ns.class_("YRM1002TagTrigger", automation.Trigger)
YRM1002TagRemovedTrigger = yrm1002_ns.class_("YRM1002TagRemovedTrigger", automation.Trigger)

YRM1002_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(YRM1002),
        cv.Optional(CONF_SCAN_DURATION, default="500ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_REPEAT_COUNT, default=10): cv.uint8_t,
        cv.Optional(CONF_TAG_PRESENT): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_ON_TAG): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(YRM1002TagTrigger),
            }
        ),
        cv.Optional(CONF_ON_TAG_REMOVED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(YRM1002TagRemovedTrigger),
            }
        ),
    }
).extend(cv.polling_component_schema("1s")).extend(uart.UART_DEVICE_SCHEMA)

CONFIG_SCHEMA = YRM1002_SCHEMA


async def setup_yrm1002(var, config):
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_scan_duration(config[CONF_SCAN_DURATION]))
    cg.add(var.set_repeat_count(config[CONF_REPEAT_COUNT]))

    if CONF_TAG_PRESENT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_TAG_PRESENT])
        cg.add(var.set_tag_present_sensor(sens))

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_ontag_trigger(trigger))
        await automation.build_automation(
            trigger, [(cg.std_string, "x"), (TagInfo, "tag")], conf
        )

    for conf in config.get(CONF_ON_TAG_REMOVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_ontagremoved_trigger(trigger))
        await automation.build_automation(
            trigger, [(cg.std_string, "x")], conf
        )


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await setup_yrm1002(var, config)
