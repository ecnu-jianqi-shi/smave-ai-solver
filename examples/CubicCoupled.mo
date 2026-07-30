model CubicCoupled
  parameter Real p = 2.0;
  Real x(start = 0.0, nominal = 4.0);
  Real y(start = 0.0, nominal = 8.0);
equation
  (x - (p + 1.0)) + 0.25 * (x - (p + 1.0)) * (x - (p + 1.0)) * (x - (p + 1.0)) + 0.1 * (y - (2.0 * p + 1.0)) = 0.0;
  (y - (2.0 * p + 1.0)) + 0.2 * (y - (2.0 * p + 1.0)) * (y - (2.0 * p + 1.0)) * (y - (2.0 * p + 1.0)) + 0.1 * (x - (p + 1.0)) = 0.0;
end CubicCoupled;
