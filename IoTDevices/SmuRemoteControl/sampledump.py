import argparse
import csv
import time
from typing import List, Tuple, Optional

from SmuInterface import SmuInterface


kRows = ['ms', 'V', 'A', 'Ah', 'J']
kAggregateMillis = 25  # max ms to consider samples part of the same row


if __name__ == "__main__":
  parser = argparse.ArgumentParser(prog='SmuCal')
  parser.add_argument('addr', type=str)
  parser.add_argument('name_prefix', type=str)

  args = parser.parse_args()

  smu = SmuInterface(args.addr)

  mac = smu.get_mac()
  print(f"Connected: {mac}")
  mac_postfix = mac.replace(':', '').lower()

  samplebuf = smu.sample_buffer()
  assert samplebuf.get() == []  # initial get to set the sample index, should be empty

  filename = f"{args.name_prefix}_{mac_postfix}.csv"

  with open(filename, 'w', newline='') as csvfile:
    csvwriter = csv.writer(csvfile, delimiter=',', quoting=csv.QUOTE_MINIMAL)
    csvwriter.writerow(kRows)
    csvfile.flush()

    last_row: Optional[Tuple[int, List[str]]] = None

    while True:
      samples = samplebuf.get()
      for sample in samples:
        if last_row is not None and last_row[0] + kAggregateMillis < sample.millis:
          csvwriter.writerow([last_row[0]] + last_row[1][1:])
          last_row = None
        if last_row is None:
          last_row = (sample.millis, [''] * len(kRows))
        last_row[1][kRows.index(sample.source)] = sample.value

      csvfile.flush()

      if samples:
        print(f"Wrote {len(samples)} samples")
      time.sleep(1)
