model BouncingBall
  parameter Real g = 9.81;
  parameter Real restitution = 0.8;
  Real h(start=1.0, nominal=1.0, min=0.0);
  Real v(start=0.0, nominal=5.0);
equation
  der(h) = v;
  der(v) = -g;
  when h <= 0.0 then
    reinit(h, 0.0);
    reinit(v, -restitution * pre(v));
  end when;
end BouncingBall;
