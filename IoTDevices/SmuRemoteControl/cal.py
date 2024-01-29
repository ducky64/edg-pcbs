import itertools
from socket import socket

import aioesphomeapi
import asyncio
import datetime
import logging
import csv
import sys


kOutputFile = 'calibration.csv'

kRecordData = [
  'UsbSMU Meas ADC Voltage',
  'UsbSMU Meas ADC Current',
  'UsbSMU Meas Voltage',
  'UsbSMU Meas Current',
]

kSetData = [
  'UsbSMU Set Voltage',
  'UsbSMU Set Current Min',
  'UsbSMU Set Current Max',
]

# Calibration with external reference
# ask_user_reference = True
# calibration_points = [  # as kSetData tuples
#   # unloaded
#   (0.0, -1, 1),
#   (1.0, -1, 1),
#   (2.0, -1, 1),
#   (4.0, -1, 1),
#   (7.0, -1, 1),
#   (10.0, -1, 1),
#   # 50 ohm
#   (0.0, -1, 1),
#   (1.0, -1, 1),
#   (2.0, -1, 1),
#   (4.0, -1, 1),
#   (7.0, -1, 1),
#   (10.0, -1, 1),
#   # 10 ohm
#   (0.0, -1, 1),
#   (1.0, -1, 1),
#   (2.0, -1, 1),
#   (4.0, -1, 1),
#   (8.0, -1, 1),
# ]

# Quick self-cal after measurements calibrated
import decimal
def drange(x, y, jump):
  while (x < y and jump > 0) or (x > y and jump < 0):
    yield float(x)
    x += decimal.Decimal(jump)

ask_user_reference = False
voltage_points = itertools.chain(*[
  drange(0, 10, 0.2),
  drange(10, 0, -0.2),
  drange(10, 0, -0.5),
  drange(0, 10, 0.5),
])
calibration_points = [  # as kSetData tuples
  (float(voltage), -0.3, 0.3)
  for voltage in voltage_points
]

state_queues: dict[int, asyncio.Queue[aioesphomeapi.SensorState]] = {}

def change_callback(state):
  """Print the state changes of the device.."""
  # print(f"{datetime.datetime.now().time()}: {state}")
  if isinstance(state, aioesphomeapi.SensorState):
    state_queue = state_queues.get(state.key, None)
    if state_queue is not None:
      if not state_queue.full():
        state_queue.put_nowait(state)

async def get_next_states(*keys: int) -> list[aioesphomeapi.SensorState]:
  """Get the next state for a given key."""
  key_queues = []
  for key in keys:
    state_queue = state_queues.get(key, None)
    if state_queue is None:
      state_queue = asyncio.Queue(maxsize=1)
      state_queues[key] = state_queue
    key_queues.append(state_queue)

  for key_queue in key_queues:
    while not key_queue.empty():
      await key_queue.get()  # flush prior entries

  values = []
  for key_queue in key_queues:
    values.append(await key_queue.get())
  return values


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


  await api.number_command(keys_by_name["UsbSMU Set Current Max"], 0.5)
  await api.number_command(keys_by_name["UsbSMU Set Current Min"], -0.1)

  await api.subscribe_states(change_callback)


  with open(kOutputFile, 'w', newline='') as csvfile:
    csvwriter = csv.writer(csvfile, delimiter=',', quoting=csv.QUOTE_MINIMAL)
    csvwriter.writerow(kSetData + kRecordData)
    csvfile.flush()

    for calibration_point in calibration_points:
      for set_name, set_value in zip(kSetData, calibration_point):
        await api.number_command(keys_by_name[set_name], set_value)
      await asyncio.sleep(0.25)

      values = [state.state for state in
        await get_next_states(*[keys_by_name[record_name] for record_name in kRecordData])]

      print(f"{calibration_point} => {values}: ")
      if ask_user_reference:
        user_data = await asyncio.to_thread(sys.stdin.readline)
        user_data_split = [elt.strip() for elt in user_data.split(',')]
      else:
        user_data_split = []

      csvwriter.writerow(list(calibration_point) + values + user_data_split)
      csvfile.flush()


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
