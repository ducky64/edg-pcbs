from typing import Tuple

import requests
import decimal

class SmuInterface:
  device_prefix = 'UsbSMU'

  kNameMeasCurrent = ' Meas Current'
  kNameMeasVoltage = ' Meas Voltage'
  kNameAdcCurrent = ' Meas ADC Current'
  kNameAdcVoltage = ' Meas ADC Voltage'
  kNameDerivPower = ' Deriv Power'
  kNameDerivEnergy = ' Deriv Eenergy'

  kNameSetCurrentMin = ' Set Current Min'
  kNameSetCurrentMax = ' Set Current Max'
  kNameSetVoltage = ' Set Voltage'
  kNameEnableRange0 = ' Range0'
  kNameEnableRange1 = ' Range1'

  def _webapi_name(self, name: str) -> str:
    # TODO should actually replace all non-alphanumeric but this is close enough
    return (self.device_prefix + name).replace(' ', '_').lower()

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

  def get_deriv_power(self) -> decimal.Decimal:
    """Returns the derived power in watts"""
    return self._get('sensor', self.kNameDerivPower)

  def get_deriv_energy(self) -> decimal.Decimal:
    """Returns the derived cumulative energy in joules"""
    return self._get('sensor', self.kNameDerivEnergy)

  def enable(self, on: bool=True, high=True) -> None:
    if on:
      action = 'turn_on'
      if high:
        names = [self.kNameEnableRange0]
      else:
        names = [self.kNameEnableRange1]
    else:
      action = 'turn_off'
      names = [self.kNameEnableRange0, self.kNameEnableRange1]

    for name in names:
      resp = requests.post(f'http://{self.addr}/switch/{self._webapi_name(name)}/{action}')
      if resp.status_code != 200:
        raise Exception(f'Request failed: {resp.status_code}')
