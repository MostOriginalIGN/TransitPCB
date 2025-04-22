// Follow this setup guide to integrate the Deno language server with your editor:
// https://deno.land/manual/getting_started/setup_your_environment
// This enables autocomplete, go to definition, etc.

// Setup type definitions for built-in Supabase Runtime APIs
import "jsr:@supabase/functions-js/edge-runtime.d.ts"

import { createClient } from 'npm:@supabase/supabase-js@2'
import GtfsRealtimeBindings from "npm:gtfs-realtime-bindings";

const supabaseUrl = Deno.env.get("SUPABASE_URL")!;
const supabaseKey = Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!;
const supabase   = createClient(supabaseUrl, supabaseKey);

const API_KEY  = Deno.env.get("API_KEY")!;
const AGENCIES = ["BA", "CT", "CE"] as const;
const URL      = "http://api.511.org/transit/tripupdates";

type Record = {
  agency_id: string;
  station_id: string;
  line_id: string;
  trip_id: string;
  arrival_time: string | null;
  departure_time: string | null;
};

async function updateAllAgencies() {
  console.log("Clearing trip_updates table…");
  const { error: clearError } = await supabase
      .from("trip_updates")
      .delete()
      .neq("agency_id", ""); 

  if (clearError) throw clearError;

  const allRecords: Record[] = [];

  for (const agency of AGENCIES) {
    const feedUrl = `${URL}?api_key=${API_KEY}&agency=${agency}`;
    console.log(`Fetching ${agency} data from ${feedUrl}`);

    const res = await fetch(feedUrl, {
      headers: { "Accept": "application/x-google-protobuf" },
    });
    if (!res.ok) {
      console.error(`Failed to fetch ${agency}: ${res.status} ${res.statusText}`);
      continue;
    }

    const buffer = await res.arrayBuffer();
    const feed   = GtfsRealtimeBindings.transit_realtime.FeedMessage.decode(
        new Uint8Array(buffer),
    );

    feed.entity.forEach((entity) => {
      const tu = entity.tripUpdate;
      if (!tu) return;

      tu.stopTimeUpdate.forEach((stu) => {
        const toIso = (sec?: number | Long) =>
            sec ? new Date(Number(sec) * 1000).toISOString() : null;

        allRecords.push({
          agency_id:     agency,
          station_id:    stu.stopId,
          line_id:       tu.trip.routeId,
          trip_id:       tu.trip.tripId,
          arrival_time:  toIso(stu.arrival?.time),
          departure_time: toIso(stu.departure?.time),
        });
      });
    });
  }

  console.log(allRecords);

  if (allRecords.length === 0) {
    return { inserted: 0 };
  }

  const { data, error } = await supabase
      .from("trip_updates")
      .insert(allRecords);

  if (error) throw error;
  return { inserted: allRecords.length, data };
}

Deno.serve(async () => {
  try {
    const result = await updateAllAgencies();
    return new Response(JSON.stringify(result), {
      headers: { "Content-Type": "application/json" },
      status: 200,
    });
  } catch (err: any) {
    console.error(err);
    return new Response(JSON.stringify({ error: err.message }), {
      headers: { "Content-Type": "application/json" },
      status: 500,
    });
  }
});
