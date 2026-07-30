model NonsymmetricPivot
  parameter Real p = 0;
  Real x1(start = 0, nominal = 1);
  Real x2(start = 0, nominal = 1);
  Real x3(start = 0, nominal = 1);
  Real x4(start = 0, nominal = 1);
equation
  p*x1 + x2 = 1;
  2*x1 + x3 = 3;
  3*x2 + x4 = 4;
  2*x3 + x4 = 3;
end NonsymmetricPivot;
