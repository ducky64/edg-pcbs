import aioesphomeapi
import asyncio

async def main():
  """Connect to an ESPHome device and get details."""

  # Establish connection
  api = aioesphomeapi.APIClient("ducky-iotusbsmu-565574.lan", port=6053, password='')
  await api.connect(login=True)

  # Get API version of the device's firmware
  print(api.api_version)

  # Show device details
  device_info = await api.device_info()
  print(device_info)

  # List all entities of the device
  entities = await api.list_entities_services()
  print(entities)

if __name__ == "__main__":
  loop = asyncio.get_event_loop()
  loop.run_until_complete(main())
