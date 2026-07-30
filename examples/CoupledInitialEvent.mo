model CoupledInitialEvent
  parameter Real u = 1;
  Real x(start = 1, nominal = 1);
  Real y(start = 0, nominal = 1);
equation
  der(x) = 0;
  der(y) = 0;
  when x >= 1 then
    reinit(y, 2);
  end when;
end CoupledInitialEvent;
