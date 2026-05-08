import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_UID

from . import CONF_YRM1002_ID, YRM1002, yrm1002_ns

DEPENDENCIES = ["yrm1002"]

YRM1002BinarySensor = yrm1002_ns.class_("YRM1002BinarySensor", binary_sensor.BinarySensor)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(YRM1002BinarySensor).extend(
    {
        cv.GenerateID(CONF_YRM1002_ID): cv.use_id(YRM1002),
        cv.Required(CONF_UID): cv.string_strict,
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)

    hub = await cg.get_variable(config[CONF_YRM1002_ID])
    cg.add(hub.register_tag(var))
    cg.add(var.set_uid(config[CONF_UID]))
