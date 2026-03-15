import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_DEVICE_CLASS, CONF_ID, DEVICE_CLASS_CONNECTIVITY

from . import Grbl, grbl_param, ns
from .const import CONF_CLIENT_CONNECTED, CONF_ENTITY_PARAM, CONF_GRBL_ID

DEPENDENCIES = ['grbl']

# bs = cg.esphome_ns.namespace('binary_sensor')

GrblBinarySensor = ns.class_("GrblBinarySensor", binary_sensor.BinarySensor, cg.PollingComponent)



CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(GrblBinarySensor), \
        cv.GenerateID(CONF_GRBL_ID): cv.use_id(Grbl),
        cv.Required(CONF_ENTITY_PARAM): cv.Any(
            cv.enum({ CONF_CLIENT_CONNECTED: CONF_CLIENT_CONNECTED }, lower=True),
            grbl_param,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GRBL_ID])
    param = config[CONF_ENTITY_PARAM]

    if param == CONF_CLIENT_CONNECTED:
        var = await binary_sensor.new_binary_sensor(config)
        if not CONF_DEVICE_CLASS in config:
            cg.add(var.set_device_class(DEVICE_CLASS_CONNECTIVITY))
        cg.add(parent.register_connection_sensor(var))

    else:
        var = cg.new_Pvariable(config[CONF_ID])
        await binary_sensor.register_binary_sensor(var, config)
        await cg.register_component(var, config)
        cg.add(var.set_parent(parent))
        cg.add(var.set_setting(param))
        cg.add(parent.register_listener(var))
