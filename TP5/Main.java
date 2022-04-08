/**
    Autors: Vincent COMMIN & Louis LEENART
 */

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

import scala.Tuple2;

import com.google.common.collect.Iterables;

import org.apache.spark.api.java.JavaPairRDD;
import org.apache.spark.api.java.JavaRDD;
import org.apache.spark.api.java.function.Function2;
import org.apache.spark.sql.Row;
import org.apache.spark.sql.SparkSession;
import org.apache.log4j.*;

/**
    PageRank algorithm applied to URLs

    Inspired by: https://en.wikipedia.org/wiki/PageRank
 */
public final class Main {
    private static class Sum implements Function2<Double, Double, Double> {
        @Override
        public Double call(Double a, Double b) {
            return a + b;
        }
    }

    private final static int INTERATIONS = 100;
    private final static double DAMPING_FACTOR = 0.85;

    public static void main(String[] args) throws Exception {
        if (args.length < 1) {
            System.out.println("usage: java -cp ... Main <json_path>");
            return;
        }

        // Spark default log level is INFO, which is not convinent for this
        // Project, so we set it to WARN only.
        Logger rootLogger = Logger.getRootLogger();
        rootLogger.setLevel(Level.WARN);
        Logger.getLogger("org.apache.spark").setLevel(Level.WARN);
        Logger.getLogger("org.spark-project").setLevel(Level.WARN);

        // Setup Spark session
        SparkSession spark = SparkSession.builder().master("local[*]").appName("PageRankTP5").getOrCreate();

        // Loads all data from json
        JavaRDD<Row> jsonRows = spark.read().json(args[0]).javaRDD();
        JavaPairRDD<Long, Iterable<Long>> links = jsonRows.mapToPair(s -> {
            Long urlID = s.getLong(s.fieldIndex("id"));
            List<Long> neighborsIDs = s.getList(s.fieldIndex("neighbors"));
            return new Tuple2<>(urlID, (Iterable<Long>) neighborsIDs);
        }).distinct().cache();

        // For benchmarking
        long startTime = System.nanoTime();

        // Init ranks
        JavaPairRDD<Long, Double> ranks = links.mapValues(rs -> 1.0);

        // Calculates and updates ranks
        for (int current = 0; current < INTERATIONS; current++) {
            // Calculates URL contributions to the rank of other URLs
            JavaPairRDD<Long, Double> contribs = links.join(ranks).values()
                    .flatMapToPair(s -> {
                        int urlCount = Iterables.size(s._1());
                        List<Tuple2<Long, Double>> results = new ArrayList<>();
                        for (Long n : s._1) {
                            results.add(new Tuple2<>(n, s._2() / urlCount));
                        }
                        return results.iterator();
                    });

            // Re-calculates URL ranks based on neighbor contributions
            ranks = contribs.reduceByKey(new Sum()).mapValues(sum -> 0.15 + sum * DAMPING_FACTOR);
        }

        // Stop time benchmark
        long elapsedTime = System.nanoTime() - startTime;

        // Print results
        List<Tuple2<Long, Double>> output = new ArrayList<Tuple2<Long, Double>>(ranks.collect());
        Comparator<Tuple2<Long, Double>> myComparator = new Comparator<Tuple2<Long, Double>>() {
            public int compare(Tuple2<Long, Double> t1, Tuple2<Long, Double> t2) {
                return Double.compare(t2._2(), t1._2());
            }
        };
        output.sort(myComparator);

        List<Row> rows = jsonRows.collect();
        for (Tuple2<Long, Double> tuple : output) {
            System.out.println(rows.get(tuple._1().intValue()).getString(3) + " : " + tuple._2() + ".");
        }

        spark.stop();
        System.err.println((double)elapsedTime / 1_000_000_000.0 + " sec");
    }
}
