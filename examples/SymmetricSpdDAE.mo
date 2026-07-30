model SymmetricSpdDAE
  Real x1(start=1);
  Real x2(start=2);
  Real y1(start=-0.1);
  Real y2(start=-0.2);
equation
  der(x1) = -x1 - y1;
  der(x2) = -x2 - y2;
  0.1*x1 + y1 = 0;
  0.1*x2 + y2 = 0;
end SymmetricSpdDAE;
