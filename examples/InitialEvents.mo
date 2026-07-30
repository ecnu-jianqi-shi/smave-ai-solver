model InitialEvents
  Real x(start = 1, nominal = 1);
  Real y(start = 0, nominal = 1);
  Real z(start = 0, nominal = 1);
equation
  der(x) = 0;
  der(y) = 0;
  der(z) = 0;
  when x >= 1 then
    reinit(y, pre(y) + 1);
  end when;
  when y >= 1 then
    reinit(z, pre(y) + 10);
  end when;
end InitialEvents;
