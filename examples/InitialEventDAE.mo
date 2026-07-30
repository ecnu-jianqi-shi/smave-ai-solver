model InitialEventDAE
  Real x(start = 0, nominal = 1);
  Real z(start = 0, nominal = 1);
  Real y(start = 0, nominal = 1);
initial equation
  x = 1;
  z = 0;
equation
  der(x) = 0;
  der(z) = 0;
  y - x - z = 0;
  when y >= 1 then
    reinit(z, pre(z) + 1);
  end when;
  when y >= 2 then
    reinit(x, pre(x) + 1);
  end when;
end InitialEventDAE;
