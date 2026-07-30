model NonsymmetricLinear
  parameter Real b1 = 3;
  parameter Real b2 = 5;
  parameter Real b3 = 5;
  parameter Real b4 = 6;
  Real x1(start = 0, nominal = 1);
  Real x2(start = 0, nominal = 1);
  Real x3(start = 0, nominal = 1);
  Real x4(start = 0, nominal = 1);
equation
  4*x1 - x2 = b1;
  2*x1 + 4*x2 - x3 = b2;
  2*x2 + 4*x3 - x4 = b3;
  2*x3 + 4*x4 = b4;
end NonsymmetricLinear;
