model RuntimeEventDAE
  Real x(start = 0, nominal = 1);
  Real z(start = 0, nominal = 1);
  Real y(start = 0, nominal = 1);
equation
  der(x) = 1;
  der(z) = 0;
  y - x - z = 0;
  when x >= 1 then
    reinit(z, pre(z) + 1);
  end when;
  when y >= 2 then
    reinit(x, 0);
  end when;
end RuntimeEventDAE;
