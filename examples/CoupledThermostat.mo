model CoupledThermostat
  parameter Real u = 1;
  Real x(start = 0, min = -2, max = 2, nominal = 1);
equation
  der(x) = u;
end CoupledThermostat;
