model CoupledSuperdense
  parameter Real u = 0;
  Real x(start = 0, nominal = 1);
  Real y(start = 0, nominal = 1);
equation
  der(x) = 0;
  der(y) = 0;
  when u >= 1 then
    reinit(x, pre(x) + 1);
  end when;
  when x >= 1 then
    reinit(y, pre(y) + 2);
  end when;
end CoupledSuperdense;
