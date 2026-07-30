model Coupled
  parameter Real p = 2.0;
  Real x(start = 1.0, nominal = 2.0, min = 0.0);
  Real y(start = 1.0, nominal = 2.0, min = 0.0);
equation
  x * x + y = p + 4.0;
  x + y * y = p + 4.0;
end Coupled;
