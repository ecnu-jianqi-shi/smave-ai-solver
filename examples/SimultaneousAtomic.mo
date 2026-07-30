model SimultaneousAtomic
  Real x(start = 0, nominal = 1);
  Real y(start = 0, nominal = 1);
  Real z(start = 0, nominal = 1);
equation
  der(x) = 1;
  der(y) = 0;
  der(z) = 0;
  when x >= 1 then
    reinit(y, pre(x) + 10);
  end when;
  when x >= 1 then
    reinit(z, pre(x) + 20);
  end when;
end SimultaneousAtomic;
