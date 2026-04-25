#include "../../include/stories.h"
#include "../../include/framebuffer.h"
#include "../../include/keyboard.h"

#ifndef NULL
#define NULL 0
#endif

#define CLR_STORY_WIN ((VGA_COLOR_WHITE << 4) | VGA_COLOR_BLACK)
#define CLR_STORY_TITLE ((VGA_COLOR_BLUE << 4) | VGA_COLOR_WHITE)
#define CLR_STORY_TEXT ((VGA_COLOR_WHITE << 4) | VGA_COLOR_BLACK)
#define CLR_STORY_STATUS ((VGA_COLOR_LIGHT_GREY << 4) | VGA_COLOR_BLACK)
#define CLR_STORY_MENU ((VGA_COLOR_CYAN << 4) | VGA_COLOR_BLACK)
#define CLR_STORY_SEL ((VGA_COLOR_BLUE << 4) | VGA_COLOR_WHITE)

static void story_draw_box(int x, int y, int w, int h, int color) {
    draw_box_vga(x, y, w, h, (color >> 4) & 0x0F);
}

static const char* story1_lines[] = {
    "THE INVENTION OF THE LIGHT BULB",
    "================================",
    "",
    "Chapter 1: Early Attempts (1800-1878)",
    "",
    "The quest for electric light began early in the 19th",
    "century. Humphry Davy created the first electric arc",
    "lamp in 1809, but it was too bright for home use.",
    "Many inventors tried to create a practical bulb.",
    "",
    "Chapter 2: Edison's Breakthrough (1879)",
    "",
    "Thomas Edison tested thousands of materials. On",
    "October 21, 1879, a carbonized bamboo filament",
    "burned for over 40 hours. Edison said, 'I have not",
    "failed. I have found 10,000 ways that won't work.'",
    "",
    "Chapter 3: Making It Practical (1880s)",
    "",
    "Edison created generators, wiring, and switches.",
    "The first power station opened on Pearl Street",
    "in 1882, serving 85 customers. This established",
    "the foundation for the modern electric grid.",
    "",
    "Chapter 4: Impact on Society",
    "",
    "Factories could operate around the clock. Streets",
    "became safer. Homes were brighter. The electric light",
    "extended productive hours and eliminated dangerous",
    "gas lamps and candles."
};

static const char* story2_lines[] = {
    "THE INVENTION OF THE TELEPHONE",
    "===============================",
    "",
    "Chapter 1: The Race to Transmit Voice (1860s-1875)",
    "",
    "For centuries, humans dreamed of speaking across",
    "distances. The telegraph sent coded messages but",
    "not the human voice. Antonio Meucci created a",
    "'talking telegraph' in 1857.",
    "",
    "Chapter 2: Bell's Success (1876)",
    "",
    "Alexander Graham Bell was a teacher of the deaf.",
    "On March 10, 1876, he called out, 'Mr. Watson,",
    "come here!' His assistant heard the words clearly",
    "through the receiver in another room.",
    "",
    "Chapter 3: The First Telephone Lines (1877-1880s)",
    "",
    "The first telephone exchange opened in New Haven,",
    "Connecticut in 1878 with 21 subscribers. Operators",
    "manually connected calls. By 1880, there were",
    "30,000 telephones in the United States.",
    "",
    "Chapter 4: Transforming Communication",
    "",
    "The telephone revolutionized business and social",
    "relationships. Families stayed connected despite",
    "distance. The industry created millions of jobs",
    "and laid groundwork for modern communications."
};

static const char* story3_lines[] = {
    "THE INVENTION OF THE AUTOMOBILE",
    "================================",
    "",
    "Chapter 1: Early Vehicles (1769-1885)",
    "",
    "The first self-propelled vehicles used steam engines.",
    "Nicolas-Joseph Cugnot built a steam tractor in 1769.",
    "Steam cars were heavy and dangerous. The internal",
    "combustion engine made practical cars possible.",
    "",
    "Chapter 2: The Benz Patent Motorwagen (1886)",
    "",
    "Karl Benz created the first practical automobile",
    "powered by a gasoline engine. His three-wheeled",
    "Motorwagen used his own four-stroke engine design.",
    "Bertha Benz made the first long-distance trip in 1888.",
    "",
    "Chapter 3: The Ford Model T (1908)",
    "",
    "Henry Ford revolutionized manufacturing with the",
    "moving assembly line. The Model T cost $825 in 1908",
    "and dropped to $260. By 1927, Ford sold 15 million.",
    "",
    "Chapter 4: The Automobile Age",
    "",
    "Cars transformed cities and created suburbs. The",
    "industry created millions of jobs. Petroleum became",
    "a strategic resource. The automobile gave people",
    "personal freedom of movement."
};

static const char* story4_lines[] = {
    "THE INVENTION OF THE AIRPLANE",
    "==============================",
    "",
    "Chapter 1: Early Aviation Dreams",
    "",
    "Humans have dreamed of flight for millennia. Leonardo",
    "da Vinci designed flying machines in the 15th century.",
    "Hot air balloons achieved flight in 1783, but they",
    "could not be steered. Gliders in the 19th century",
    "proved that controlled flight was possible.",
    "",
    "Chapter 2: The Wright Brothers (1903)",
    "",
    "Orville and Wilbur Wright were bicycle mechanics from",
    "Ohio. They studied birds and built wind tunnels to test",
    "wing designs. On December 17, 1903, at Kitty Hawk,",
    "North Carolina, their Flyer achieved powered flight.",
    "The first flight lasted 12 seconds and covered 120 feet.",
    "",
    "Chapter 3: Developing Aviation (1900s-1910s)",
    "",
    "The Wright brothers improved their designs. By 1905,",
    "they could fly for over 30 minutes. In 1908, they",
    "demonstrated their machine publicly. World War I",
    "accelerated aircraft development for military use.",
    "",
    "Chapter 4: Impact on the World",
    "",
    "Aviation transformed transportation, warfare, and",
    "global commerce. The airplane shrank the world,",
    "making international travel routine. Air mail,",
    "airlines, and airports became essential infrastructure."
};

static const char* story5_lines[] = {
    "THE INVENTION OF THE RADIO",
    "===========================",
    "",
    "Chapter 1: The Science of Wireless (1860s-1890s)",
    "",
    "James Clerk Maxwell predicted electromagnetic waves",
    "in 1864. Heinrich Hertz demonstrated them in 1887.",
    "Scientists knew wireless communication was possible,",
    "but it required practical application.",
    "",
    "Chapter 2: Marconi's Wireless Telegraph (1895)",
    "",
    "Guglielmo Marconi, an Italian inventor, built the",
    "first practical radio system in 1895. He sent a",
    "wireless signal over a mile. By 1901, he transmitted",
    "across the Atlantic Ocean from England to Canada.",
    "",
    "Chapter 3: Radio Broadcasting (1920s)",
    "",
    "The first commercial radio station, KDKA in",
    "Pittsburgh, began broadcasting in 1920. Radio brought",
    "news, music, and entertainment into homes. By 1930,",
    "millions of families owned radio sets.",
    "",
    "Chapter 4: Radio's Influence",
    "",
    "Radio transformed entertainment and politics.",
    "Presidents could speak directly to the nation.",
    "It united the country during the Great Depression",
    "and World War II. Radio was the first mass medium."
};

static const char* story6_lines[] = {
    "THE INVENTION OF THE PHONOGRAPH",
    "================================",
    "",
    "Chapter 1: Capturing Sound",
    "",
    "For most of human history, music was ephemeral -",
    "performed once and lost forever. The ability to",
    "record and replay sound was a revolutionary concept",
    "that seemed like magic to people of the 1870s.",
    "",
    "Chapter 2: Edison's Tinfoil Phonograph (1877)",
    "",
    "Thomas Edison invented the phonograph in 1877.",
    "He wrapped tinfoil around a cylinder. A needle",
    "etched sound waves into the foil. When played",
    "back, it reproduced the sound. Edison recited",
    "'Mary Had a Little Lamb' as the first recording.",
    "",
    "Chapter 3: The Gramophone and Records",
    "",
    "Emile Berliner invented the gramophone in 1887,",
    "using flat discs instead of cylinders. These",
    "became known as records. By the early 1900s,",
    "recorded music became a popular entertainment.",
    "",
    "Chapter 4: Transforming Music",
    "",
    "The phonograph preserved performances for",
    "posterity. It allowed music to reach millions",
    "of homes. It created the recording industry",
    "and changed how musicians worked forever."
};

static const char* story7_lines[] = {
    "THE INVENTION OF THE MOTION PICTURE CAMERA",
    "===========================================",
    "",
    "Chapter 1: Early Motion Photography",
    "",
    "Photography was invented in 1839. In the 1870s,",
    "Eadweard Muybridge used multiple cameras to",
    "capture motion in sequence. These were still",
    "photos, not true moving pictures.",
    "",
    "Chapter 2: The Lumiere Brothers (1895)",
    "",
    "Auguste and Louis Lumiere invented the Cinematographe",
    "in 1895 in France. It could record, develop, and",
    "project motion pictures. They held the first public",
    "screening in Paris, showing workers leaving a factory.",
    "",
    "Chapter 3: Edison's Kinetoscope",
    "",
    "Thomas Edison developed the Kinetoscope in 1891.",
    "It was a peephole viewer for one person. Edison",
    "built the first film studio, the Black Maria, in",
    "1893 to produce motion pictures commercially.",
    "",
    "Chapter 4: Birth of Cinema",
    "",
    "Motion pictures became a major entertainment medium.",
    "By 1900, nickelodeons showed films to millions.",
    "Hollywood became the center of film production.",
    "Movies became the dominant art form of the 20th century."
};

static const char* story8_lines[] = {
    "THE INVENTION OF THE VACUUM CLEANER",
    "====================================",
    "",
    "Chapter 1: Cleaning Before Electricity",
    "",
    "Before the vacuum cleaner, floors were swept with",
    "brooms or beaten to remove dust. Carpets were hung",
    "outside and beaten with sticks. It was laborious",
    "work that took hours to complete.",
    "",
    "Chapter 2: Ives McGaffey's Carpet Sweeper (1869)",
    "",
    "The first carpet sweeper was invented in 1869 in",
    "Chicago. It used a hand crank to rotate brushes",
    "that picked up dirt. It was mechanical, not powered.",
    "It made cleaning easier but still required effort.",
    "",
    "Chapter 3: The Electric Vacuum (1901-1908)",
    "",
    "Hubert Cecil Booth invented the first powered vacuum",
    "cleaner in 1901 in England. It was large and horse-",
    "drawn. James Spangler invented the portable electric",
    "vacuum in 1907. He sold his patent to William Hoover.",
    "",
    "Chapter 4: Impact on Homes",
    "",
    "The vacuum cleaner made home cleaning much easier.",
    "It allowed wall-to-wall carpeting to become popular.",
    "The Hoover company became synonymous with vacuum",
    "cleaners. It changed domestic life worldwide."
};

static const char* story9_lines[] = {
    "THE INVENTION OF THE WASHING MACHINE",
    "=====================================",
    "",
    "Chapter 1: Washing Clothes by Hand",
    "",
    "Before washing machines, laundry was done by hand.",
    "Clothes were scrubbed on washboards, then rinsed",
    "and wrung out. It was backbreaking work that took",
    "an entire day each week for most families.",
    "",
    "Chapter 2: Early Mechanical Washers",
    "",
    "The first washing machine was invented in 1797",
    "as a hand-cranked device. In 1851, James King",
    "patented a machine with a drum. These early",
    "machines were still manually powered.",
    "",
    "Chapter 3: The Electric Washing Machine (1908)",
    "",
    "The Thor was the first electric washing machine,",
    "introduced in 1908 by the Hurley Machine Company.",
    "It had an electric motor to turn the drum. Alva",
    "Fisher is credited with inventing the electric washer.",
    "",
    "Chapter 4: Liberating Housewives",
    "",
    "The washing machine saved countless hours of labor.",
    "It was one of the most important appliances for",
    "liberating women from household drudgery. Along",
    "with other appliances, it transformed domestic life."
};

static const char* story10_lines[] = {
    "THE INVENTION OF THE REFRIGERATOR",
    "==================================",
    "",
    "Chapter 1: Keeping Food Cold",
    "",
    "Before refrigeration, food was preserved by salting,",
    "smoking, or drying. Ice was cut from frozen lakes in",
    "winter and stored in ice houses. Ice boxes kept food",
    "cool with blocks of ice delivered by the iceman.",
    "",
    "Chapter 2: Early Refrigeration (1850s-1870s)",
    "",
    "Ferdinand Carré invented the absorption refrigerator",
    "in 1859. Early commercial refrigeration used ammonia",
    "or other toxic gases. These machines were large and",
    "dangerous, used mainly by businesses.",
    "",
    "Chapter 3: The Electric Refrigerator (1913)",
    "",
    "Fred W. Wolf invented the domestic electric refrigerator",
    "in 1913. Nathaniel B. Wales introduced the Kelvinator",
    "in 1918. Frigidaire, founded in 1918, made the first",
    "self-contained refrigerator in 1923. Freon made",
    "refrigeration safer in the 1930s.",
    "",
    "Chapter 4: Transforming Food",
    "",
    "Refrigeration made fresh food available year-round.",
    "It allowed supermarkets to exist. It reduced food",
    "spoilage and disease. The refrigerator became an",
    "essential appliance in every home."
};

static const char* story11_lines[] = {
    "THE INVENTION OF THE ASSEMBLY LINE",
    "===================================",
    "",
    "Chapter 1: Early Manufacturing",
    "",
    "Before the assembly line, skilled craftsmen built",
    "products from start to finish. Each worker made",
    "an entire product. This was slow and required",
    "highly trained workers. Products were expensive",
    "and varied in quality.",
    "",
    "Chapter 2: Interchangeable Parts",
    "",
    "Eli Whitney introduced interchangeable parts for",
    "guns in 1801. This meant parts could be swapped",
    "between products. It was a step toward mass",
    "production but still required skilled assembly.",
    "",
    "Chapter 3: Ford's Moving Assembly Line (1913)",
    "",
    "Henry Ford installed the first moving assembly",
    "line at his Highland Park plant in 1913. Workers",
    "stayed in place while cars moved past them. Each",
    "worker performed one simple task repeatedly. This",
    "cut the time to build a Model T from 12 hours",
    "to just 93 minutes.",
    "",
    "Chapter 4: Impact on Industry",
    "",
    "The assembly line revolutionized manufacturing.",
    "Products became affordable for the masses. It",
    "created the modern industrial economy. While it",
    "made work repetitive, it also created millions",
    "of jobs and transformed society."
};

static const char* story12_lines[] = {
    "THE INVENTION OF THE X-RAY",
    "===========================",
    "",
    "Chapter 1: Discovery (1895)",
    "",
    "Wilhelm Röntgen was experimenting with cathode",
    "rays when he noticed a fluorescent screen glowing",
    "in his darkened laboratory. The rays passed through",
    "paper, wood, and even his wife's hand. He called",
    "them X-rays, using X for the unknown.",
    "",
    "Chapter 2: Medical Revolution",
    "",
    "Within months, X-rays were being used in medicine.",
    "Doctors could see broken bones and foreign objects",
    "inside the body without surgery. This revolutionized",
    "diagnosis and treatment. The first Nobel Prize in",
    "Physics went to Röntgen in 1901.",
    "",
    "Chapter 3: Developing Technology",
    "",
    "Early X-ray machines were primitive and dangerous.",
    "Operators often suffered radiation burns. Over time,",
    "safer equipment was developed. The Coolidge tube",
    "in 1913 made X-rays more reliable and controllable.",
    "",
    "Chapter 4: Modern Applications",
    "",
    "X-rays became essential in medicine, dentistry,",
    "and industry. CT scans, developed in the 1970s,",
    "created 3D images. X-ray crystallography revealed",
    "the structure of DNA. Röntgen's discovery changed",
    "science and medicine forever."
};

static const char* story13_lines[] = {
    "THE INVENTION OF PENICILLIN",
    "============================",
    "",
    "Chapter 1: The Discovery (1928)",
    "",
    "Alexander Fleming returned from vacation to find",
    "his petri dishes contaminated with mold. He noticed",
    "the bacteria around the mold had died. The mold",
    "was Penicillium notatum. Fleming had discovered",
    "the first antibiotic, though he didn't realize",
    "its full potential at first.",
    "",
    "Chapter 2: Developing the Drug",
    "",
    "Howard Florey and Ernst Chain at Oxford University",
    "developed penicillin as a drug during World War II.",
    "They struggled to produce enough for testing. The",
    "first patient was treated in 1941, but supplies ran",
    "out and he died.",
    "",
    "Chapter 3: Mass Production",
    "",
    "The US government made penicillin production a",
    "priority for the war effort. By D-Day in 1944,",
    "enough was available to treat all Allied wounded.",
    "Production methods improved rapidly, making the",
    "drug widely available by 1945.",
    "",
    "Chapter 4: The Antibiotic Age",
    "",
    "Penicillin saved millions of lives from bacterial",
    "infections that were previously fatal. It launched",
    "the antibiotic revolution. Fleming, Florey, and Chain",
    "shared the Nobel Prize in 1945. Antibiotics became",
    "one of medicine's greatest achievements."
};

static const char* story14_lines[] = {
    "THE INVENTION OF THE JET ENGINE",
    "================================",
    "",
    "Chapter 1: Early Concepts",
    "",
    "The idea of jet propulsion dates back centuries.",
    "Chinese rockets used the principle. As aircraft",
    "approached the speed of sound with propellers,",
    "they encountered limits. A new form of propulsion",
    "was needed for faster flight.",
    "",
    "Chapter 2: Whittle's Breakthrough (1930s)",
    "",
    "Frank Whittle, a British RAF officer, patented",
    "the turbojet engine in 1930. He built the first",
    "working engine in 1937. Meanwhile, Hans von Ohain",
    "in Germany independently developed a jet engine",
    "that flew in 1939, beating Whittle's first flight.",
    "",
    "Chapter 3: Wartime Development",
    "",
    "Germany deployed the first operational jet fighter,",
    "the Me 262, in 1944. It was faster than any Allied",
    "plane. Britain's Gloster Meteor also saw service.",
    "Jet engines gave aircraft unprecedented speed and",
    "altitude performance.",
    "",
    "Chapter 4: The Jet Age",
    "",
    "After the war, jet engines transformed aviation.",
    "Commercial jets like the de Havilland Comet (1949)",
    "and Boeing 707 (1958) made air travel faster and",
    "more accessible. The jet age had begun, shrinking",
    "the world and changing how people travel."
};

static const char* story15_lines[] = {
    "THE INVENTION OF THE TRANSISTOR",
    "================================",
    "",
    "Chapter 1: The Need for Something Better",
    "",
    "Vacuum tubes powered early electronics but had",
    "problems. They were large, hot, fragile, and",
    "consumed lots of power. A smaller, more reliable",
    "alternative was needed. Bell Labs set out to",
    "find a solution using semiconductors.",
    "",
    "Chapter 2: The Breakthrough (1947)",
    "",
    "John Bardeen, Walter Brattain, and William Shockley",
    "at Bell Labs invented the transistor in 1947. They",
    "discovered that applying voltage to a semiconductor",
    "could control current flow. The point-contact",
    "transistor was born, later improved to junction",
    "transistors.",
    "",
    "Chapter 3: Commercial Development",
    "",
    "The first commercial transistor radio, the Regency",
    "TR-1, appeared in 1954. It cost $50 but showed the",
    "transistor's potential. Texas Instruments and other",
    "companies refined manufacturing. Transistors soon",
    "replaced tubes in most applications.",
    "",
    "Chapter 4: The Electronic Revolution",
    "",
    "Transistors enabled portable electronics, space",
    "exploration, and modern computers. They led to",
    "integrated circuits and microprocessors. The three",
    "inventors won the Nobel Prize in 1956. The",
    "transistor is considered one of the greatest",
    "inventions of the 20th century."
};

static const char* story16_lines[] = {
    "THE INVENTION OF THE MICROWAVE OVEN",
    "====================================",
    "",
    "Chapter 1: Accidental Discovery (1946)",
    "",
    "Percy Spencer, an engineer at Raytheon, was working",
    "on radar equipment using magnetrons. He noticed a",
    "candy bar in his pocket had melted. He realized",
    "microwave radiation was generating heat. This led",
    "to experiments with popcorn and eggs.",
    "",
    "Chapter 2: First Microwave Oven",
    "",
    "Raytheon built the first microwave oven, the Radarange,",
    "in 1947. It stood 6 feet tall, weighed 750 pounds,",
    "and cost $5,000. It was water-cooled and too large",
    "for home use. Restaurants and ships were the first",
    "customers for this industrial version.",
    "",
    "Chapter 3: Home Microwave (1967)",
    "",
    "The Amana Radarange, introduced in 1967, was the",
    "first affordable home microwave. It cost $495 and",
    "fit on a countertop. By the 1970s, prices dropped",
    "and sales soared. The microwave became a kitchen",
    "essential in the 1980s.",
    "",
    "Chapter 4: Transforming Cooking",
    "",
    "Microwaves made cooking faster and easier. They",
    "changed food packaging with microwave-ready meals.",
    "While some criticized the cooking quality, the",
    "convenience was undeniable. The microwave became",
    "one of the most popular appliances ever invented."
};

static const char* story17_lines[] = {
    "THE INVENTION OF THE TELEVISION",
    "================================",
    "",
    "Chapter 1: Early Development (1920s)",
    "",
    "Multiple inventors worked on television simultaneously.",
    "John Logie Baird demonstrated mechanical TV in 1926.",
    "Philo Farnsworth invented electronic television",
    "in 1927, at age 21. Vladimir Zworykin developed the",
    "iconoscope camera tube for RCA.",
    "",
    "Chapter 2: First Broadcasts",
    "",
    "The BBC began regular TV broadcasts in 1936.",
    "NBC started in the US in 1939. World War II halted",
    "development, but post-war saw rapid growth. By 1950,",
    "millions of American homes had televisions. Radio",
    "stations added TV broadcasting.",
    "",
    "Chapter 3: Color Television",
    "",
    "Color TV was demonstrated in the 1920s but took",
    "decades to perfect. RCA introduced the first practical",
    "color system in 1953. It was backward compatible",
    "with black and white sets. Color adoption was slow",
    "until the mid-1960s.",
    "",
    "Chapter 4: Television's Impact",
    "",
    "Television transformed entertainment, news, and",
    "politics. It brought the world into living rooms.",
    "The moon landing in 1969 was watched by 600 million",
    "people. TV changed how we consume information and",
    "became the dominant medium of the 20th century."
};

static const char* story18_lines[] = {
    "THE INVENTION OF SYNTHETIC RUBBER",
    "==================================",
    "",
    "Chapter 1: Natural Rubber Limitations",
    "",
    "Natural rubber came from rubber trees in tropical",
    "regions. It was essential for tires, hoses, and",
    "countless products. Supplies were limited and prices",
    "volatile. Countries sought alternatives, especially",
    "for military use during wartime.",
    "",
    "Chapter 2: Early Synthetics",
    "",
    "Chemists developed synthetic rubber in the early",
    "1900s. Germany produced methyl rubber during WWI",
    "when natural supplies were cut off. These early",
    "versions were inferior to natural rubber and",
    "expensive to produce in quantity.",
    "",
    "Chapter 3: WWII and GR-S Rubber",
    "",
    "When Japan cut off natural rubber supplies in 1942,",
    "the US launched an emergency program. Scientists",
    "developed GR-S (Government Rubber-Styrene). By 1944,",
    "US factories produced over 700,000 tons annually,",
    "outproducing the natural rubber industry.",
    "",
    "Chapter 4: Modern Synthetic Rubber",
    "",
    "Today, most rubber is synthetic. Different types",
    "serve different needs - SBR for tires, neoprene",
    "for oil resistance, silicone for heat resistance.",
    "Synthetic rubber made automobiles, space flight,",
    "and modern industry possible."
};

static const char* story19_lines[] = {
    "THE INVENTION OF THE ELECTRIC GUITAR",
    "=====================================",
    "",
    "Chapter 1: The Need for Volume",
    "",
    "Acoustic guitars couldn't compete with brass and",
    "reed instruments in big bands. Guitarists sought",
    "ways to amplify their instruments. Early attempts",
    "put microphones near guitars or attached phonograph",
    "needles to bridges with limited success.",
    "",
    "Chapter 2: First Electric Guitars (1930s)",
    "",
    "George Beauchamp and Adolph Rickenbacker created",
    "the 'Frying Pan' lap steel guitar in 1931. It had",
    "a magnetic pickup that converted string vibrations",
    "to electrical signals. Hollow-body electric guitars",
    "followed, used by jazz guitarists.",
    "",
    "Chapter 3: The Solid Body (1950s)",
    "",
    "Leo Fender introduced the Broadcaster (later",
    "Telecaster) in 1950. It had a solid body to reduce",
    "feedback. The Gibson Les Paul followed in 1952.",
    "These guitars defined the sound of rock and roll",
    "and popular music.",
    "",
    "Chapter 4: Revolutionizing Music",
    "",
    "The electric guitar transformed popular music.",
    "It enabled rock and roll, blues, and jazz to",
    "reach larger audiences. Guitar heroes emerged as",
    "major stars. The electric guitar became an icon",
    "of 20th century culture."
};

static const char* story20_lines[] = {
    "THE INVENTION OF THE LASER",
    "===========================",
    "",
    "Chapter 1: Theoretical Foundation",
    "",
    "Albert Einstein predicted stimulated emission in",
    "1917. This was the theoretical basis for lasers.",
    "Charles Townes invented the maser (microwave",
    "amplification) in 1953. Scientists realized the",
    "same principle could work with visible light.",
    "",
    "Chapter 2: First Laser (1960)",
    "",
    "Theodore Maiman built the first working laser at",
    "Hughes Research Laboratories in 1960. He used a",
    "ruby crystal to produce coherent red light. The",
    "laser produced an intense, focused beam unlike",
    "any light source before it.",
    "",
    "Chapter 3: Early Applications",
    "",
    "Initially called 'a solution looking for a problem',",
    "lasers found uses quickly. They cut and welded",
    "materials with precision. Barcode scanners, laser",
    "printers, and medical instruments soon followed.",
    "Communications research led to fiber optics.",
    "",
    "Chapter 4: Ubiquitous Technology",
    "",
    "Today, lasers are everywhere. They read DVDs, scan",
    "groceries, perform eye surgery, guide missiles,",
    "and enable internet communications. The laser, once",
    "a laboratory curiosity, became essential to modern",
    "technology."
};

static void story_display(const char** lines, int line_count) {
    int scroll = 0;
    int max_scroll = line_count - 17;
    if (max_scroll < 0) max_scroll = 0;
    int last_scroll = -1;
    
    // Initial draw
    restore_background();
    story_draw_box(5, 2, 70, 21, CLR_STORY_WIN);
    story_draw_box(5, 2, 70, 1, CLR_STORY_TITLE);
    set_cursor(30, 2);
    print_string_color("Famous Inventions", CLR_STORY_TITLE);
    print_char_at('X', CLR_STORY_TITLE, 73, 2);
    story_draw_box(5, 23, 70, 1, CLR_STORY_STATUS);
    set_cursor(7, 23);
    print_string_color("Arrows: Scroll | ESC: Back", CLR_STORY_STATUS);
    
    while (1) {
        // Only redraw content if scroll changed
        if (scroll != last_scroll) {
            // Clear content area
            for (int i = 0; i < 17; i++) {
                story_draw_box(7, 4 + i, 66, 1, CLR_STORY_WIN);
            }
            
            // Draw visible lines
            for (int i = 0; i < 17; i++) {
                int idx = scroll + i;
                if (idx < line_count) {
                    set_cursor(7, 4 + i);
                    print_string_color(lines[idx], CLR_STORY_TEXT);
                }
            }
            
            // Update scroll indicator
            if (max_scroll > 0) {
                int thumb = (scroll * 15) / (max_scroll + 1);
                if (thumb > 15) thumb = 15;
                draw_box_vga(74, 4 + thumb, 1, 2, VGA_COLOR_BLUE);
            }
            
            last_scroll = scroll;
            swap_buffers();
        }
        
        char key = get_key();
        if (key == 27) break; // ESC
        else if (key == KEY_UP) { // Up
            if (scroll > 0) scroll--;
        } else if (key == KEY_DOWN) { // Down
            if (scroll < max_scroll) scroll++;
        }
    }
}

void stories_app() {
    int selection = 0;
    int scroll = 0;
    int total_stories = 20;
    int last_selection = -1;
    int last_scroll = -1;
    
    const char* menu[] = {
        "1. The Light Bulb (1879)",
        "2. The Telephone (1876)",
        "3. The Automobile (1886)",
        "4. The Airplane (1903)",
        "5. The Radio (1895)",
        "6. The Phonograph (1877)",
        "7. Motion Picture Camera (1895)",
        "8. The Vacuum Cleaner (1901)",
        "9. The Washing Machine (1908)",
        "10. The Refrigerator (1913)",
        "11. The Assembly Line (1913)",
        "12. The X-Ray (1895)",
        "13. Penicillin (1928)",
        "14. The Jet Engine (1930s)",
        "15. The Transistor (1947)",
        "16. The Microwave Oven (1946)",
        "17. The Television (1920s)",
        "18. Synthetic Rubber (1942)",
        "19. The Electric Guitar (1931)",
        "20. The Laser (1960)"
    };
    
    // Initial draw
    restore_background();
    story_draw_box(5, 2, 70, 21, CLR_STORY_WIN);
    story_draw_box(5, 2, 70, 1, CLR_STORY_TITLE);
    set_cursor(30, 2);
    print_string_color("Famous Inventions", CLR_STORY_TITLE);
    print_char_at('X', CLR_STORY_TITLE, 73, 2);
    story_draw_box(5, 23, 70, 1, CLR_STORY_STATUS);
    set_cursor(15, 23);
    print_string_color("Arrows: Select | ENTER: Read | ESC: Exit", CLR_STORY_STATUS);
    swap_buffers();
    
    while (1) {
        // Only redraw if selection or scroll changed
        if (selection != last_selection || scroll != last_scroll) {
            int visible_items = 17;
            for (int i = 0; i < visible_items; i++) {
                int idx = scroll + i;
                if (idx < total_stories) {
                    int color = (selection == idx) ? CLR_STORY_SEL : CLR_STORY_MENU;
                    story_draw_box(7, 4 + i, 66, 1, color);
                    set_cursor(9, 4 + i);
                    print_string_color(menu[idx], color);
                }
            }
            
            // Update scroll indicator
            if (total_stories > visible_items) {
                int thumb = (scroll * 15) / (total_stories - visible_items + 1);
                if (thumb > 15) thumb = 15;
                draw_box_vga(74, 4 + thumb, 1, 2, VGA_COLOR_BLUE);
            }
            
            last_selection = selection;
            last_scroll = scroll;
            swap_buffers();
        }
        
        char key = get_key();
        if (key == 27) break; // ESC
        else if (key == KEY_UP) { // Up
            if (selection > 0) {
                selection--;
                if (selection < scroll) scroll = selection;
            }
        } else if (key == KEY_DOWN) { // Down
            if (selection < total_stories - 1) {
                selection++;
                if (selection >= scroll + 17) scroll = selection - 16;
            }
        } else if (key == '\n') { // Enter
            switch (selection) {
                case 0: story_display(story1_lines, sizeof(story1_lines) / sizeof(char*)); break;
                case 1: story_display(story2_lines, sizeof(story2_lines) / sizeof(char*)); break;
                case 2: story_display(story3_lines, sizeof(story3_lines) / sizeof(char*)); break;
                case 3: story_display(story4_lines, sizeof(story4_lines) / sizeof(char*)); break;
                case 4: story_display(story5_lines, sizeof(story5_lines) / sizeof(char*)); break;
                case 5: story_display(story6_lines, sizeof(story6_lines) / sizeof(char*)); break;
                case 6: story_display(story7_lines, sizeof(story7_lines) / sizeof(char*)); break;
                case 7: story_display(story8_lines, sizeof(story8_lines) / sizeof(char*)); break;
                case 8: story_display(story9_lines, sizeof(story9_lines) / sizeof(char*)); break;
                case 9: story_display(story10_lines, sizeof(story10_lines) / sizeof(char*)); break;
                case 10: story_display(story11_lines, sizeof(story11_lines) / sizeof(char*)); break;
                case 11: story_display(story12_lines, sizeof(story12_lines) / sizeof(char*)); break;
                case 12: story_display(story13_lines, sizeof(story13_lines) / sizeof(char*)); break;
                case 13: story_display(story14_lines, sizeof(story14_lines) / sizeof(char*)); break;
                case 14: story_display(story15_lines, sizeof(story15_lines) / sizeof(char*)); break;
                case 15: story_display(story16_lines, sizeof(story16_lines) / sizeof(char*)); break;
                case 16: story_display(story17_lines, sizeof(story17_lines) / sizeof(char*)); break;
                case 17: story_display(story18_lines, sizeof(story18_lines) / sizeof(char*)); break;
                case 18: story_display(story19_lines, sizeof(story19_lines) / sizeof(char*)); break;
                case 19: story_display(story20_lines, sizeof(story20_lines) / sizeof(char*)); break;
            }
            // Redraw menu after returning from story
            last_selection = -1;
            last_scroll = -1;
        }
    }
    
    clear_screen();
}

