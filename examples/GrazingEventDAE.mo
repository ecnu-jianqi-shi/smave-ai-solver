model GrazingEventDAE
  Real x(start = -0.9, nominal = 1);
  Real z(start = 0, nominal = 1);
  Real y(start = -0.9, nominal = 1);
equation
  der(x) = 1;
  der(z) = 0;
  y - x = 0;
  when y * y <= 0 then
    reinit(z, pre(z) + 1);
  end when;
end GrazingEventDAE;
