model IndexTwoConstraint
  Real q(start=0.2);
  Real v(start=1);
  Real lambda(start=0);
equation
  der(q) = v + lambda;
  der(v) = -q;
  q = 0;
end IndexTwoConstraint;
