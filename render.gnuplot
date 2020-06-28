set term png size 800,400 giant
set output "piece_size_cdf.png"
set title "piece sizes"
set ylabel "torrents (%)"
set xlabel "piece size in kiB"
set xtic 2
set logscale x
set key off
set xrange [16:8192]
set grid ytics
plot "piece_size.dat" using 1:2 title "piece size" with steps

set terminal postscript
set output "piece_size_cdf.ps"
replot

set term png size 800,400 giant
set output "torrent_size_cdf.png"
set title "torrent payload sizes"
set ylabel "torrents (%)"
set xlabel "torrent payload size (MiB)"
set logscale x
set xtic auto
set xrange [10:100000]
set grid mxtics xtics ytics
plot "size.dat" using 1:2 title "torrent payload size" with lines

set terminal postscript
set output "torrent_size_cdf.ps"
replot

set term png size 800,400 giant
set output "metadata_size_cdf.png"
set title "torrent metadata sizes"
set ylabel "torrents (%)"
set xlabel "torrent metadata size (kiB)"
set logscale x
set xtic auto
set xrange [1:*]
set grid mxtics xtics ytics
plot "metadata_size.dat" using 1:2 title "torrent metadata size" with lines

set terminal postscript
set output "metadata_size_cdf.ps"
replot

