model GroupFallback
  parameter Real p = 2.0;
  Real x(start=100.0, nominal=2.0, min=0.0);
equation
  x*x = p;
end GroupFallback;
