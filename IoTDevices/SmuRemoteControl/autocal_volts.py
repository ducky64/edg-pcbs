import argparse
import sys
import time
import csv
from decimal import Decimal
from typing import Tuple, List

from SmuInterface import SmuInterface
from calutil import regress


kOutputFile = 'calibration.csv'
kSetReadDelay = 0.5  # seconds

kVoltageCalPoints = [  # as voltage, current min, current max
  # don't calibrate zero, might be off-scale on output mode

  "Open load",
  (1.0, -0.1, 0.1),
  (2.0, -0.1, 0.1),
  (4.0, -0.1, 0.1),
  (6.0, -0.1, 0.1),
  (8.0, -0.1, 0.1),
  (10.0, -0.1, 0.1),
  (14.0, -0.1, 0.1),
  (18.0, -0.1, 0.1),
  (1.0, -0.1, 0.1),

  # "Connect 50ohm",
  # (1.0, -0.1, 0.1),
  # (2.0, -0.1, 0.1),
  # (4.0, -0.1, 0.1),
  # (8.0, -0.1, 0.3),
  # (12.0, -0.1, 0.5),
  # (16.0, -0.1, 0.5),
  # (20.0, -0.1, 0.5),
  # (1.0, -0.1, 0.1),

  # "Connect 10ohm",
  # (1.0, -0.1, 0.2),
  # (2.0, -0.1, 0.4),
  # (4.0, -0.1, 0.6),
  # (6.0, -0.1, 1.0),
  # (8.0, -0.1, 1.0),
  # (1.0, -0.1, 0.2),
]

if __name__ == "__main__":
  parser = argparse.ArgumentParser(prog='SmuCal')
  parser.add_argument('addr', type=str)
  args = parser.parse_args()

  smu = SmuInterface(args.addr)

  prev_factor, prev_offset = smu.cal_get_voltage_meas()
  print(f'Current voltage meas cal: {prev_factor} x + {prev_offset}')
  prev_factor, prev_offset = smu.cal_get_voltage_set()
  print(f'Current voltage set cal: {prev_factor} x + {prev_offset}')

  while True:
    print('Clear and re-run calibration? [y/n]: ', end='')
    user_data = input()
    if user_data.lower() == 'y':
      break
    elif user_data.lower() == 'n':
      sys.exit()

  smu.cal_set_voltage_meas(1, 0)
  smu.cal_set_voltage_set(1, 0)
  time.sleep(kSetReadDelay)

  with open(kOutputFile, 'w', newline='') as csvfile:
    csvwriter = csv.writer(csvfile, delimiter=',', quoting=csv.QUOTE_MINIMAL)
    csvwriter.writerow([
      'set_voltage', 'set_current_min', 'set_current_max',
      'adc_voltage', 'adc_current', 'meas_voltage', 'meas_current',
      'ref_voltage', 'ref_current'
    ])
    csvfile.flush()

    cal_rows = []
    meas_voltage_cal_data: List[Tuple[Decimal, Decimal]] = []  # device-measured, external-measured (reference)
    set_voltage_cal_data: List[Tuple[Decimal, Decimal]] = []  # setpoint, external-measured (reference)

    enabled = False
    for calibration_point in kVoltageCalPoints:
      if isinstance(calibration_point, str):
        print(calibration_point, end='')
        input()
      else:
        (set_voltage, set_current_min, set_current_max) = calibration_point
        smu.set_current_limits(set_current_min, set_current_max)
        smu.set_voltage(set_voltage)
        if not enabled:
          smu.enable(True)
          enabled = True

        time.sleep(kSetReadDelay)

        meas_voltage, meas_current = smu.get_voltage_current()
        adc_voltage, adc_current = smu.get_raw_voltage_current()
        values = [adc_voltage, adc_current, meas_voltage, meas_current]

        print(f"{calibration_point}, MV={meas_voltage}, MI={meas_current}", end='')
        print(': ', end='')
        user_data = input()

        row = list(calibration_point) + values + [str(user_data), '']
        csvwriter.writerow(row)
        cal_rows.append(row)
        csvfile.flush()

        meas_voltage_cal_data.append((meas_voltage, Decimal(user_data)))
        set_voltage_cal_data.append((Decimal(set_voltage), Decimal(user_data)))

    smu.enable(False)

    # voltage linear regression
    print("Voltage meas calibration")
    meas_cal_factor, meas_cal_offset = regress(
      [float(pt[0]) for pt in meas_voltage_cal_data], [float(pt[1]) for pt in meas_voltage_cal_data])

    print("Voltage set calibration")
    set_cal_factor, set_cal_offset = regress(
      [float(pt[0]) for pt in set_voltage_cal_data], [float(pt[1]) for pt in set_voltage_cal_data])

  while True:
    print('Commit to device? [y/n]: ', end='')
    user_data = input()
    if user_data.lower() == 'y':
      break
    elif user_data.lower() == 'n':
      sys.exit()

  smu.cal_set_voltage_meas(meas_cal_factor, meas_cal_offset)
  smu.cal_set_voltage_set(set_cal_factor, set_cal_offset)
  print("Wrote device calibration. Allow 5 seconds to commit to flash before power cycling.")
