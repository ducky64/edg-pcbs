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
  # "Connect 50ohm, low range meter",
  (3, -0.1, 0.025),
  (3, -0.1, 0.05),
  (3, -0.1, 0.075),
  (3, -0.1, 0.1),
  (3, -0.1, 0.125),
  (3, -0.1, 0.15),
  (3, -0.1, 0.175),
  (3, -0.1, 0.2),
  (3, -0.1, 0.225),
  (3, -0.1, 0.25),
  (3, -0.1, 0.275),
  (3, -0.1, 0.3),
  (3, -0.1, 0.325),
  (3, -0.1, 0.35),

  # "Connect 50ohm",
  # (3, -0.1, 0.1),
  # (3, -0.1, 0.2),
  # (3, -0.1, 0.3),
  # (3, -0.1, 0.5),
  # (6, -0.1, 1.0),
  # (8.5, -0.1, 1.5),
  # (11, -0.1, 2.0),
  # (13.5, -0.1, 2.5),
  # (16, -0.1, 3.0),

  # "Connect 10ohm",
  # (3, -0.1, 0.2),
  # (6, -0.1, 0.5),
  # (9, -0.1, 0.8),
  # (11, -0.1, 1),
  # (16, -0.1, 1.5),
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
          smu.enable(True, False)
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

    # voltage linear regression
    print("Current meas calibration")
    regress([float(pt[0]) for pt in meas_current_cal_data], [float(pt[1]) for pt in meas_current_cal_data])

    print("Current set calibration")
    regress([float(pt[0]) for pt in set_current_cal_data], [float(pt[1]) for pt in set_current_cal_data])
