model RuntimeRankLossDAE
  Real x(start = 0, nominal = 1);
  Real y(start = 0, nominal = 1);
equation
  der(x) = 0;
  (0.2 - time) * y = 0;
end RuntimeRankLossDAE;
