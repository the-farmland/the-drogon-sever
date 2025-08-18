

-- Create users table for request tracking
-- Create or update users table
CREATE TABLE IF NOT EXISTS users (
    user_identifier VARCHAR(255) PRIMARY KEY,
    req INTEGER DEFAULT 0,
    res INTEGER DEFAULT 0,
    blocked BOOLEAN DEFAULT FALSE,
    last_updated TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Function to log a request
CREATE OR REPLACE FUNCTION log_user_request(p_userid TEXT) RETURNS VOID AS $$
BEGIN
    INSERT INTO users (user_identifier, req, res, last_updated)
    VALUES (p_userid, 1, 0, CURRENT_TIMESTAMP)
    ON CONFLICT (user_identifier)
    DO UPDATE SET 
        req = users.req + 1,
        last_updated = CURRENT_TIMESTAMP;
END;
$$ LANGUAGE plpgsql;

-- Function to log a response
CREATE OR REPLACE FUNCTION log_user_response(p_userid TEXT) RETURNS VOID AS $$
BEGIN
    UPDATE users
    SET res = res + 1,
        last_updated = CURRENT_TIMESTAMP
    WHERE user_identifier = p_userid;
END;
$$ LANGUAGE plpgsql;

-- Function to check if user is blocked (added missing function with example logic)
CREATE OR REPLACE FUNCTION is_user_blocked(p_userid TEXT) RETURNS BOOLEAN AS $$
DECLARE
    user_req INT;
    user_res INT;
    user_blocked BOOLEAN;
BEGIN
    SELECT req, res, blocked INTO user_req, user_res, user_blocked
    FROM users
    WHERE user_identifier = p_userid;
    
    IF NOT FOUND THEN
        RETURN FALSE;
    END IF;
    
    -- Example logic: block if requests exceed responses by 10
    IF user_req > user_res + 10 THEN
        UPDATE users SET blocked = TRUE WHERE user_identifier = p_userid;
        RETURN TRUE;
    END IF;
    
    RETURN user_blocked;
END;
$$ LANGUAGE plpgsql;

-- Create locations table
CREATE TABLE IF NOT EXISTS locations (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    country TEXT NOT NULL,
    state TEXT,
    description TEXT,
    svg_link TEXT,
    rating NUMERIC(3,1) DEFAULT 0.0
);

-- Create function to get top locations
CREATE OR REPLACE FUNCTION get_top_locations(limit_count INTEGER)
RETURNS SETOF locations AS $$
BEGIN
    RETURN QUERY
    SELECT * FROM locations
    ORDER BY rating DESC
    LIMIT limit_count;
END;
$$ LANGUAGE plpgsql;

-- Create function to get location by ID
CREATE OR REPLACE FUNCTION get_location_by_id(location_id TEXT)
RETURNS SETOF locations AS $$
BEGIN
    RETURN QUERY
    SELECT * FROM locations
    WHERE id = location_id;
END;
$$ LANGUAGE plpgsql;

-- Create function to search locations
CREATE OR REPLACE FUNCTION search_locations(search_query TEXT)
RETURNS SETOF locations AS $$
BEGIN
    RETURN QUERY
    SELECT * FROM locations
    WHERE name ILIKE '%' || search_query || '%'
       OR country ILIKE '%' || search_query || '%'
       OR state ILIKE '%' || search_query || '%'
    ORDER BY rating DESC;
END;
$$ LANGUAGE plpgsql;

-- Create index for better search performance
CREATE INDEX IF NOT EXISTS idx_locations_search ON locations 
USING gin(to_tsvector('english', name || ' ' || country || ' ' || state));

-- Sample data (optional - for testing)
INSERT INTO locations (id, name, country, state, description, svg_link, rating)
VALUES 
    ('nyc', 'New York City', 'United States', 'New York', 'The city that never sleeps', 'https://example.com/nyc.svg', 4.8),
    ('paris', 'Paris', 'France', NULL, 'City of love', 'https://example.com/paris.svg', 4.9),
    ('tokyo', 'Tokyo', 'Japan', NULL, 'Vibrant capital of Japan', 'https://example.com/tokyo.svg', 4.7)

ON CONFLICT (id) DO NOTHING;
