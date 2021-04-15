local module = {}

module.WIFI = {}
module.WIFI.ssid = "UAS_PRDKU"
module.WIFI.pwd = "44444444"

module.HOST = "m15.cloudmqtt.com"
module.PORT = 12774
module.USERNAME = "nodemcu"
module.PASSWORD = "kudakecilbanget"
module.ID = node.chipid()

module.ENDPOINT = "/nodemcu/"
return module
