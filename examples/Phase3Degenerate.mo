model Phase3Degenerate
  parameter Real p = 1.0;
  Real x(start = 1.0, nominal = 1.0);
equation
  x * x = p * p;
end Phase3Degenerate;
