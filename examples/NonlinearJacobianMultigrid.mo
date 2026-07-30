model NonlinearJacobianMultigrid
  parameter Real b1 = 1; Real x1(start = 1);
  parameter Real b2 = 1; Real x2(start = 1);
  parameter Real b3 = 1; Real x3(start = 1);
  parameter Real b4 = 1; Real x4(start = 1);
  parameter Real b5 = 1; Real x5(start = 1);
  parameter Real b6 = 1; Real x6(start = 1);
  parameter Real b7 = 1; Real x7(start = 1);
  parameter Real b8 = 1; Real x8(start = 1);
equation
  x1*x1*x1 + 1.5*x1 - x2 = b1;
  x2*x2*x2 + 1.5*x2 - x1 - x3 = b2;
  x3*x3*x3 + 1.5*x3 - x2 - x4 = b3;
  x4*x4*x4 + 1.5*x4 - x3 - x5 = b4;
  x5*x5*x5 + 1.5*x5 - x4 - x6 = b5;
  x6*x6*x6 + 1.5*x6 - x5 - x7 = b6;
  x7*x7*x7 + 1.5*x7 - x6 - x8 = b7;
  x8*x8*x8 + 1.5*x8 - x7 = b8;
end NonlinearJacobianMultigrid;
