model GrazingContact
  Real x(start = -0.9, nominal = 1);
  Real y(start = 0, nominal = 1);
equation
  der(x) = 1;
  der(y) = 0;
  when x * x <= 0 then
    reinit(y, pre(y) + 1);
  end when;
end GrazingContact;
