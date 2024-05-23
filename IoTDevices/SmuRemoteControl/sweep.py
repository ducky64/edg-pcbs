import argparse
import time
import csv
from decimal import Decimal
from typing import Tuple, List

import numpy as np

from SmuInterface import SmuInterface
from calutil import regress


kOutputFile = 'sweep.csv'
kSetReadDelay = 0.4  # seconds

if __name__ == "__main__":
  parser = argparse.ArgumentParser(prog='SmuCal')
  parser.add_argument('addr', type=str)
  args = parser.parse_args()

  smu = SmuInterface(args.addr)

  with open(kOutputFile, 'w', newline='') as csvfile:
    csvwriter = csv.writer(csvfile, delimiter=',', quoting=csv.QUOTE_MINIMAL)
    csvwriter.writerow([
      'set_voltage', 'set_current_min', 'set_current_max',
      'adc_voltage', 'adc_current', 'meas_voltage', 'meas_current'
    ])
    csvfile.flush()

    cal_rows = []
    meas_voltage_cal_data: List[Tuple[Decimal, Decimal]] = []  # device-measured, external-measured (reference)
    set_voltage_cal_data: List[Tuple[Decimal, Decimal]] = []  # setpoint, external-measured (reference)

    enabled = False
    for voltage in range(0, 20*50):
      setpoint = (set_voltage, set_current_min, set_current_max) = (voltage/50, -0.1, 0.1)
      smu.set_current_limits(set_current_min, set_current_max)
      smu.set_voltage(set_voltage)
      if not enabled:
        smu.enable(True, False)
        enabled = True

      time.sleep(kSetReadDelay)

      meas_voltage, meas_current = smu.get_voltage_current()
      adc_voltage, adc_current = smu.get_raw_voltage_current()
      values = [adc_voltage, adc_current, meas_voltage, meas_current]

      print(f"SV={voltage}, MV={meas_voltage}, MI={meas_current}")

      row = list(setpoint) + values
      csvwriter.writerow(row)
      cal_rows.append(row)
      csvfile.flush()

    smu.enable(False)
