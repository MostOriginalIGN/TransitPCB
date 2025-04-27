// Follow this setup guide to integrate the Deno language server with your editor:
// https://deno.land/manual/getting_started/setup_your_environment
// This enables autocomplete, go to definition, etc.

// Setup type definitions for built-in Supabase Runtime APIs
import "jsr:@supabase/functions-js/edge-runtime.d.ts"
import { createClient } from 'npm:@supabase/supabase-js@2'

const supabaseUrl = Deno.env.get("SUPABASE_URL")!;
const supabaseKey = Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!;
const supabase   = createClient(supabaseUrl, supabaseKey);

type Record = {
  agency_id: string;
  station_id: string;
  line_id: string;
  trip_id: string;
  arrival_time: string | null;
  departure_time: string | null;
};

Deno.serve(async () => {
  const now       = Date.now()
  const windowStart = new Date(now - 30 * 60 * 1000).toISOString()
  const windowEnd   = new Date(now + 45 * 60 * 1000).toISOString()

  const { data, error } = await supabase
      .from("trip_updates")
      .select("agency_id,station_id,line_id,trip_id,arrival_time,departure_time")
      .or(
          `and(arrival_time.gte.${windowStart},arrival_time.lte.${windowEnd}),` +
          `and(departure_time.gte.${windowStart},departure_time.lte.${windowEnd})`
      )

  function makeJsonResponse(bodyObj: unknown, status = 200) {
    const bodyStr = JSON.stringify(bodyObj)
    const len     = new TextEncoder().encode(bodyStr).length
    const headers = new Headers({
      "Content-Type":   "application/json",
      "Content-Length": len.toString(),
    })
    return new Response(bodyStr, { status, headers })
  }

  if (error) {
    console.error("Error fetching data:", error)
    return makeJsonResponse({ error: "Failed to fetch data" }, 500)
  }

  return makeJsonResponse(data, 200)
})