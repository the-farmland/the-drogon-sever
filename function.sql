

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

-- File: function.sql

-- (Existing functions and tables from your original file are assumed to be here)
-- ...

-- Add a 'boards' column to the locations table to store scoreboard/leaderboard data.
-- Using JSONB is efficient for storing and querying structured, schemaless data.
ALTER TABLE locations ADD COLUMN IF NOT EXISTS boards JSONB;

-- Create a new table 'charts' for tracking supplementary data for events,
-- matches, or elections, linked to a location.
CREATE TABLE IF NOT EXISTS charts (
    id SERIAL PRIMARY KEY,
    location_id TEXT NOT NULL REFERENCES locations(id) ON DELETE CASCADE,
    chart_type VARCHAR(100) NOT NULL, -- e.g., 'match_statistics', 'lap_times', 'voter_demographics'
    title VARCHAR(255) NOT NULL,
    chart_data JSONB NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- Create an index on location_id for faster lookups in the 'charts' table.
CREATE INDEX IF NOT EXISTS idx_charts_location_id ON charts(location_id);

-- Upsert the "Grand Prix Hungary 2025" location with its race results.
-- The 'boards' data includes a leaderboard structure.
INSERT INTO locations (id, name, country, description, boards)
VALUES 
    ('grand-prix-hungary-2025', 'Grand Prix Hungary 2025', 'Hungary', 'Formula 1 racing event at the Hungaroring.',
     '{
        "title": "Race Results",
        "type": "F1_LEADERBOARD",
        "drivers": [
            {"position": 1, "name": "Driver A", "team": "Team X", "time": "1:35:24.112"},
            {"position": 2, "name": "Driver B", "team": "Team Y", "time": "1:35:32.543"},
            {"position": 3, "name": "Driver C", "team": "Team Z", "time": "1:35:45.987"}
        ]
      }')
ON CONFLICT (id) DO UPDATE SET
    name = EXCLUDED.name,
    country = EXCLUDED.country,
    description = EXCLUDED.description,
    boards = EXCLUDED.boards;

-- Upsert the FIFA 2026 match location with its initial game board.
-- The 'boards' data shows a match score structure.
INSERT INTO locations (id, name, country, description, boards)
VALUES 
    ('fifa26-mex-grupa-1', 'Match 1 - Group A (Mexico #1) – Estadio Azteca Mexico City', 'Mexico', 'Opening match for Group A in the FIFA World Cup 2026.',
     '{
        "title": "Match Score",
        "type": "FOOTBALL_MATCH",
        "status": "Scheduled",
        "teams": {
            "home": {"name": "Mexico", "score": 0},
            "away": {"name": "TBD", "score": 0}
        },
        "details": "Kick-off: TBD"
      }')
ON CONFLICT (id) DO UPDATE SET
    name = EXCLUDED.name,
    country = EXCLUDED.country,
    description = EXCLUDED.description,
    boards = EXCLUDED.boards;

-- Add sample chart data for the FIFA match location.
-- This demonstrates how to store related but separate data visualizations.
INSERT INTO charts (location_id, chart_type, title, chart_data)
VALUES
    ('fifa26-mex-grupa-1', 'team_head_to_head', 'Historical Performance: Mexico vs TBD', 
     '{
        "mexico_wins": 5,
        "tbd_wins": 3,
        "draws": 2
     }')
ON CONFLICT (id) DO NOTHING;




