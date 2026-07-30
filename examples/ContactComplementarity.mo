model ContactComplementarity
  Real lambda1(start=0);
  Real lambda2(start=0);
  Real lambda3(start=0);
equation
  complementarity(lambda1, 2*lambda1-lambda2-1);
  complementarity(lambda2, -lambda1+2*lambda2-lambda3+0.25);
  complementarity(lambda3, -lambda2+2*lambda3-0.5);
end ContactComplementarity;
