import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from . import CONF_YRM1002_ID, YRM1002

DEPENDENCIES = ["yrm1002"]

CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.GenerateID(CONF_YRM1002_ID): cv.use_id(YRM1002),
    }
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)

    hub = await cg.get_variable(config[CONF_YRM1002_ID])
    cg.add(hub.set_text_sensor(var))
