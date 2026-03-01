import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_DEVICE_CLASS, CONF_ID, DEVICE_CLASS_CONNECTIVITY

from . import CONF_GRBL_ID, Grbl
from .const import CONF_ENTITY_PARAM, CONF_CLIENT_CONNECTED

DEPENDENCIES = ['grbl']

ns = cg.esphome_ns.namespace('binary_sensor')

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend({
    cv.GenerateID(CONF_GRBL_ID): cv.use_id(Grbl),
    cv.Required(CONF_ENTITY_PARAM): cv.enum({
        CONF_CLIENT_CONNECTED: 'client_connected',
    }),
})


def to_code(config):
    print(config)
    parent = yield cg.get_variable(config[CONF_GRBL_ID])
    var = cg.new_Pvariable(config[CONF_ID])

    yield binary_sensor.register_binary_sensor(var, config)

    device_class = config.get(CONF_DEVICE_CLASS)

    match config[CONF_ENTITY_PARAM]:
        case 'client_connected':
            cg.add(parent.register_connection_sensor(var))
            if device_class is None:
                 device_class = DEVICE_CLASS_CONNECTIVITY

    if device_class is not None:
        cg.add(var.set_device_class(device_class))
