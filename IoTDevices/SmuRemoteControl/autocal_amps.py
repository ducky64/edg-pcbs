import argparse
import time
import csv
from decimal import Decimal
from typing import Tuple, List

import numpy as np

from SmuInterface import SmuInterface
from calutil import regress


kOutputFile = 'calibration.csv'
kSetReadDelay = 0.5  # seconds

kVoltageCalPoints = [  # as voltage, current min, current max
  # "Connect 50ohm",  # with setpoint cal
  # (3, -0.1, 0.01),
  # (3, -0.1, 0.02),
  # (3, -0.1, 0.03),
  # (3, -0.1, 0.05),
  # (6, -0.1, 0.1),
  # (8.5, -0.1, 0.15),
  # (11, -0.1, 0.20),
  # (13.5, -0.1, 0.25),
  # (16, -0.1, 0.30),

  # "Connect 50ohm",  # measurement cal only
  # (1.0, -0.1, 0.5),
  # (2.0, -0.1, 0.5),
  # (4.0, -0.1, 0.5),
  # (8.0, -0.1, 0.5),
  # (12.0, -0.1, 0.5),
  # (16.0, -0.1, 0.75),
  # (20.0, -0.1, 0.75),
  # (1.0, -0.1, 0.5),

  "Connect 10ohm",
  (6, -0.1, 0.5),
  (9, -0.1, 0.8),
  (11, -0.1, 1),
  (16, -0.1, 1.5),

  # "Connect 10ohm",  # measurement cal only
  # (3, -0.1, 0.8),
  # (6, -0.1, 1.1),
  # (9, -0.1, 1.4),
  # (11, -0.1, 1.6),
  # (16, -0.1, 2.1),
  # (3, -0.1, 0.8),
]

if __name__ == "__main__":
  parser = argparse.ArgumentParser(prog='SmuCal')
  parser.add_argument('addr', type=str)
  args = parser.parse_args()

  smu = SmuInterface(args.addr)

  with open(kOutputFile, 'w', newline='') as csvfile:
    csvwriter = csv.writer(csvfile, delimiter=',', quoting=csv.QUOTE_MINIMAL)
    csvwriter.writerow([
      'set_voltage', 'set_current_min', 'set_current_max',
      'adc_voltage', 'adc_current', 'meas_voltage', 'meas_current',
      'ref_voltage', 'ref_current'
    ])
    csvfile.flush()

    cal_rows = []
    meas_current_cal_data: List[Tuple[Decimal, Decimal]] = []  # device-measured, external-measured (reference)
    set_current_cal_data: List[Tuple[Decimal, Decimal]] = []  # setpoint, external-measured (reference)

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
          # smu.enable(True, False)  # low range
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

        meas_current_cal_data.append((meas_current, Decimal(user_data)))
        set_current_cal_data.append((Decimal(set_current_max), Decimal(user_data)))

    smu.enable(False)

    # linear regression
    print("Current meas calibration")
    regress([float(pt[0]) for pt in meas_current_cal_data], [float(pt[1]) for pt in meas_current_cal_data])

    print("Current set calibration")
    regress([float(pt[0]) for pt in set_current_cal_data], [float(pt[1]) for pt in set_current_cal_data])
