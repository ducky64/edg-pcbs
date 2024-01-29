from socket import socket

import aioesphomeapi
import asyncio
import datetime
import logging

state_queues: dict[int, asyncio.Queue[aioesphomeapi.SensorState]] = {}

def change_callback(state):
  """Print the state changes of the device.."""
  # print(f"{datetime.datetime.now().time()}: {state}")
  if isinstance(state, aioesphomeapi.SensorState):
    state_queue = state_queues.get(state.key, None)
    if state_queue is not None:
      if not state_queue.full():
        state_queue.put_nowait(state)

async def get_next_state(key: int) -> aioesphomeapi.SensorState:
  """Get the next state for a given key."""
  state_queue = state_queues.get(key, None)
  if state_queue is None:
    state_queue = asyncio.Queue(maxsize=1)
    state_queues[key] = state_queue
  while not state_queue.empty():
    await state_queue.get()  # flush prior entries
  return await state_queue.get()


async def main():
  """Connect to an ESPHome device and get details."""

  # Establish connection
  api = aioesphomeapi.APIClient("ducky-iotusbsmu-565574.lan", port=6053, password=None)
  await api.connect(login=True)
  # logging.basicConfig(level=logging.DEBUG)
  # api.set_debug(True)  # must be set after connection is made

  # Get API version of the device's firmware
  print(api.api_version)

  # Show device details
  device_info = await api.device_info()
  print(device_info)

  # List all entities of the device
  sensors, services = await api.list_entities_services()
  keys_by_name = dict((sensor.name, sensor.key) for sensor in sensors)
  print(sensors)
  print(services)

  await api.number_command(keys_by_name["UsbSMU Set Voltage"], 0)
  await api.number_command(keys_by_name["UsbSMU Set Current Max"], 0.5)
  await api.number_command(keys_by_name["UsbSMU Set Current Min"], -0.1)

  await api.subscribe_states(change_callback)

  volts = (await get_next_state(keys_by_name["UsbSMU Meas Voltage"])).state
  print(volts)


if __name__ == "__main__":
  loop = asyncio.get_event_loop()
  try:
    # asyncio.ensure_future(main())
    # loop.run_forever()

    loop.run_until_complete(main())
  except KeyboardInterrupt:
    pass
  finally:
    loop.close()
