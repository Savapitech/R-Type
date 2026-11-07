function update_velocity(t, dt, vx, vy)
    local new_vx = -220.0

    local new_vy = math.sin(t * 3.5) * 160.0

    return new_vx, new_vy
end
