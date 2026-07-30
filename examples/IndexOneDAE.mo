model IndexOneDAE
  parameter Real k = 2;
  Real x(start = 0, nominal = 1);
  Real y(start = 0, nominal = 1);
initial equation
  x = 1;
equation
  der(x) = -y;
  y - k * x = 0;
end IndexOneDAE;
