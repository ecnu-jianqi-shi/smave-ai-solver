model LinearChain
  parameter Real b1 = 1.0;
  parameter Real b2 = 1.0;
  parameter Real b3 = 1.0;
  parameter Real b4 = 1.0;
  parameter Real b5 = 1.0;
  parameter Real b6 = 1.0;
  parameter Real b7 = 1.0;
  parameter Real b8 = 1.0;
  Real x1(start = 0.0);
  Real x2(start = 0.0);
  Real x3(start = 0.0);
  Real x4(start = 0.0);
  Real x5(start = 0.0);
  Real x6(start = 0.0);
  Real x7(start = 0.0);
  Real x8(start = 0.0);
equation
  4*x1 - x2 = b1;
  -x1 + 4*x2 - x3 = b2;
  -x2 + 4*x3 - x4 = b3;
  -x3 + 4*x4 - x5 = b4;
  -x4 + 4*x5 - x6 = b5;
  -x5 + 4*x6 - x7 = b6;
  -x6 + 4*x7 - x8 = b7;
  -x7 + 4*x8 = b8;
end LinearChain;
