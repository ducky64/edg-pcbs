from typing import Tuple

import requests
import decimal

class SmuInterface:
  kNameMeasCurrent = 'UsbSMU Meas Current'
  kNameMeasVoltage = 'UsbSMU Meas Voltage'
  kNameAdcCurrent = 'UsbSMU Meas ADC Current'
  kNameAdcVoltage = 'UsbSMU Meas ADC Voltage'

  kNameSetCurrentMin = 'UsbSMU Set Current Min'
  kNameSetCurrentMax = 'UsbSMU Set Current Max'
  kNameSetVoltage = 'UsbSMU Set Voltage'
  kNameEnable = 'UsbSMU Range0'

  @staticmethod
  def _webapi_name(name: str) -> str:
    # TODO should actually replace all non-alphanumeric but this is close enough
    return name.replace(' ', '_').lower()

  def _set(self, service: str, name: str, value: float) -> None:
    resp = requests.post(f'http://{self.addr}/{service}/{self._webapi_name(name)}/set?value={value}')
    if resp.status_code != 200:
      raise Exception(f'Request failed: {resp.status_code}')

  def _get(self, service: str, name: str) -> decimal.Decimal:
    resp = requests.get(f'http://{self.addr}/{service}/{self._webapi_name(name)}')
    if resp.status_code != 200:
      raise Exception(f'Request failed: {resp.status_code}')
    return decimal.Decimal(resp.json()['state'].split(' ')[0])

  def __init__(self, addr: str):
    self.addr = addr

  def set_voltage(self, voltage: float) -> None:
    self._set('number', self.kNameSetVoltage, voltage)

  def set_current_limits(self, current_min: float, current_max: float) -> None:
    assert current_min < current_max
    self._set('number', self.kNameSetCurrentMin, current_min)
    self._set('number', self.kNameSetCurrentMax, current_max)

  def get_voltage_current(self) -> Tuple[decimal.Decimal, decimal.Decimal]:
    """Returns the measured voltage and current"""
    return (self._get('sensor', self.kNameMeasVoltage),
            self._get('sensor', self.kNameMeasCurrent))

  def get_raw_voltage_current(self) -> Tuple[int, int]:
    """Returns the voltage and current ADC counts"""
    return (int(self._get('sensor', self.kNameAdcVoltage)),
            int(self._get('sensor', self.kNameAdcCurrent)))

  def enable(self, on: bool=True) -> None:
    if on:
      action = 'turn_on'
    else:
      action = 'turn_off'
    resp = requests.post(f'http://{self.addr}/switch/{self._webapi_name(self.kNameEnable)}/{action}')
    if resp.status_code != 200:
      raise Exception(f'Request failed: {resp.status_code}')
