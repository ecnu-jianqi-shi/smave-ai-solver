model EquationRoutingNonsymmetric
  parameter Real b1 = 1;
  parameter Real b2 = 2;
  parameter Real b3 = 3;
  parameter Real b4 = 4;
  Real x1(start = 0);
  Real x2(start = 0);
  Real x3(start = 0);
  Real x4(start = 0);
equation
  2*x1 + x2 = b1;
  3*x2 + x3 = b2;
  4*x3 + x4 = b3;
  x1 + 5*x4 = b4;
end EquationRoutingNonsymmetric;
