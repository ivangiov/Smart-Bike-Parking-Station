local module = {}

local function wifi_wait_ip()
  if wifi.sta.getip()== nil then
    print("IP unavailable, Waiting...")
  else
    tmr.stop(1)
    print("\n====================================")
    print("ESP8266 mode is: " .. wifi.getmode())
    print("Chip ID "..node.chipid());
    print("MAC address is: " .. wifi.ap.getmac())
    print("IP is "..wifi.sta.getip())
    print("====================================")
    app.start()
  end
end

local function wifi_start(list_aps)
    if list_aps then
      wifi.setmode(wifi.STATION);
      wifi.sta.config(config.WIFI)
      wifi.sta.connect()
      print("Connecting to " .. config.WIFI.ssid .. " ...")
      --config.SSID = nil  -- can save memory
      tmr.alarm(1, 2500, 1, wifi_wait_ip)
    else
        print("Error getting AP list")
    end
end

function module.start()
  print("Configuring Wifi ...")
  wifi.setmode(wifi.STATION);
  wifi.sta.getap(wifi_start)
end

return module
