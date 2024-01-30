import argparse
import itertools
import time
import csv
from typing import Tuple

from SmuInterface import SmuInterface


kOutputFile = 'calibration.csv'
kSetReadDelay = 0.2  # seconds


import decimal
def drange(x, y, jump):
  x = decimal.Decimal(x)
  y = decimal.Decimal(y)
  while (x < y and jump > 0) or (x > y and jump < 0):
    yield float(x)
    x += decimal.Decimal(jump)


# Calibration with external reference
# ask_user_reference = True
# calibration_points = [  # as voltage, current min, current max
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


# Quick voltage self-cal after measurements calibrated
ask_user_reference = False
voltage_points = itertools.chain(*[
  drange(0, 2.6, 0.1),
  # drange(7, 0, -0.1),
])
calibration_points = [  # as kSetData tuples
  (float(voltage), -1, 0.02)
  for voltage in voltage_points
]


# Quick current self-cal after measurements calibrated
# ask_user_reference = False
# current_points = itertools.chain(*[
#   drange(0, 1, 0.025),
#   drange(1, 0, -0.025),
# ])
# calibration_points = [  # as kSetData tuples
#   (12, -0.1, current)
#   for current in current_points
# ]



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


if __name__ == "__main__":
  parser = argparse.ArgumentParser(prog='SmuCal')
  parser.add_argument('addr', type=str)
  parser.add_argument('--plot', action='store_true')
  args = parser.parse_args()

  smu = SmuInterface(args.addr)

  with open(kOutputFile, 'w', newline='') as csvfile:
    csvwriter = csv.writer(csvfile, delimiter=',', quoting=csv.QUOTE_MINIMAL)
    csvwriter.writerow([
      'set_voltage', 'set_current_min', 'set_current_max',
      'adc_voltage', 'adc_current', 'meas_voltage', 'meas_current',
    ])
    csvfile.flush()

    cal_rows = []
    smu.enable(True)

    for calibration_point in calibration_points:
      (set_voltage, set_current_min, set_current_max) = calibration_point
      smu.set_current_limits(set_current_min, set_current_max)
      smu.set_voltage(set_voltage)

      time.sleep(kSetReadDelay)

      meas_voltage, meas_current = smu.get_voltage_current()
      adc_voltage, adc_current = smu.get_raw_voltage_current()
      values = [adc_voltage, adc_current, meas_voltage, meas_current]

      print(f"{calibration_point} => {values}", end='')

      if ask_user_reference:
        print(': ', end='')
        user_data = input()
        user_data_split = [elt.strip() for elt in user_data.split(',')]
      else:
        user_data_split = []
        print('')  # endline

      row = list(calibration_point) + values + user_data_split
      csvwriter.writerow(row)
      cal_rows.append(row)
      csvfile.flush()

    smu.enable(False)

    if args.plot:
      import matplotlib.pyplot as plt
      def index_to_color(idx: int, max_idx: int) -> Tuple[float, float, float]:
        component = ((max_idx - idx) / (max_idx) * 0.8) + 0.1
        return (component, component, component)

      xs = [row[5] for row in cal_rows]  # meas_voltage
      ys = [row[6] for row in cal_rows]  # meas_current
      cs = [index_to_color(idx, len(cal_rows) - 1)
            for idx, row in enumerate(cal_rows)]
      plt.plot(xs, ys)  # line
      plt.scatter(xs, ys, c=cs)  # index-coded marker
      plt.show()
