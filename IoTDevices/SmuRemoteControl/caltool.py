import argparse
import json

from SmuInterface import SmuInterface


if __name__ == "__main__":
  parser = argparse.ArgumentParser(prog='SmuCal')
  parser.add_argument('addr', type=str)
  parser.add_argument('name_prefix', type=str)
  parser.add_argument('--dump', action='store_true')
  args = parser.parse_args()

  smu = SmuInterface(args.addr)

  mac = smu.get_mac()
  print(f"Connected: {mac}")
  mac_postfix = mac.replace(':', '').lower()

  cal_dict = smu.cal_get_all()
  cal_str_dict = {key: str(value) for key, value in cal_dict.items()}
  print("Current calibration:")
  for key, value in cal_dict.items():
    print(f"  {key}: {value}")

  if args.dump:
    filename = f"{args.name_prefix}_{mac_postfix}.json"
    jsonstr = json.dumps(cal_str_dict, indent=2)
    with open(filename, 'w') as f:
      f.write(jsonstr)

    print(f"Wrote to {filename}")
