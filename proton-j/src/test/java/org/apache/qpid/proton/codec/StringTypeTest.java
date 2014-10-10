/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */
package org.apache.qpid.proton.codec;

import static org.junit.Assert.assertEquals;

import java.lang.Character.UnicodeBlock;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

import org.junit.Test;
import org.apache.qpid.proton.amqp.messaging.AmqpValue;

/**
 * Test the encoding and decoding of {@link StringType} values.
 */
public class StringTypeTest
{
    private static final Charset CHARSET_UTF8 = Charset.forName("UTF-8");

    /**
     * Loop over all the chars in given {@link UnicodeBlock}s and return a
     * {@link Set <String>} containing all the possible values as their
     * {@link String} values.
     *
     * @param blocks the {@link UnicodeBlock}s to loop over
     * @return a {@link Set <String>} containing all the possible values as
     * {@link String} values
     */
    private static Set<String> getAllStringsFromUnicodeBlocks(final UnicodeBlock... blocks)
    {
        final Set<UnicodeBlock> blockSet = new HashSet<UnicodeBlock>(Arrays.asList(blocks));
        final Set<String> strings = new HashSet<String>();
        for (int codePoint = 0; codePoint <= Character.MAX_CODE_POINT; codePoint++)
        {
            if (blockSet.contains(UnicodeBlock.of(codePoint)))
            {
                final int charCount = Character.charCount(codePoint);
                final StringBuilder sb = new StringBuilder(
                        charCount);
                if (charCount == 1)
                {
                    sb.append(String.valueOf((char) codePoint));
                }
                else if (charCount == 2)
                {
                    //TODO: use Character.highSurrogate(codePoint) and Character.lowSurrogate(codePoint) when Java 7 is baseline
                    char highSurrogate = (char) ((codePoint >>> 10) + ('\uD800' - (0x010000 >>> 10)));
                    char lowSurrogate =  (char) ((codePoint & 0x3ff) + '\uDC00');

                    sb.append(highSurrogate);
                    sb.append(lowSurrogate);
                }
                else
                {
                    throw new IllegalArgumentException("Character.charCount of "
                                                       + charCount + " not supported.");
                }
                strings.add(sb.toString());
            }
        }
        return strings;
    }


    /**
     * Test the encoding and decoding of various complicated Unicode characters
     * which will end up as "surrogate pairs" when encoded to UTF-8
     */
    @Test
    public void calculateUTF8Length()
    {
        for (final String input : generateTestData())
        {
            assertEquals("Incorrect string length calculated for string '"+input+"'",input.getBytes(CHARSET_UTF8).length, StringType.calculateUTF8Length(input));
        }
    }

    /**
     * Test the encoding and decoding of various  Unicode characters
     */
    @Test
    public void encodeDecodeStrings()
    {
        final DecoderImpl decoder = new DecoderImpl();
        final EncoderImpl encoder = new EncoderImpl(decoder);
        AMQPDefinedTypes.registerAllTypes(decoder, encoder);
        final ByteBuffer bb = ByteBuffer.allocate(16);

        for (final String input : generateTestData())
        {
            bb.clear();
            final AmqpValue inputValue = new AmqpValue(input);
            encoder.setByteBuffer(bb);
            encoder.writeObject(inputValue);
            bb.clear();
            decoder.setByteBuffer(bb);
            final AmqpValue outputValue = (AmqpValue) decoder.readObject();
            assertEquals("Failed to round trip String correctly: ", input, outputValue.getValue());
        }
    }

    // build up some test data with a set of suitable Unicode characters
    private Set<String> generateTestData()
    {
        return new HashSet<String>()
            {
                private static final long serialVersionUID = 7331717267070233454L;

                {
                    // non-surrogate pair blocks
                    addAll(getAllStringsFromUnicodeBlocks(UnicodeBlock.BASIC_LATIN,
                                                         UnicodeBlock.LATIN_1_SUPPLEMENT,
                                                         UnicodeBlock.GREEK,
                                                         UnicodeBlock.LETTERLIKE_SYMBOLS));
                    // blocks with surrogate pairs
                    //TODO: restore others when Java 7 is baseline
                    addAll(getAllStringsFromUnicodeBlocks(/*UnicodeBlock.MISCELLANEOUS_SYMBOLS_AND_PICTOGRAPHS,*/
                                                         UnicodeBlock.MUSICAL_SYMBOLS,
                                                         /*UnicodeBlock.EMOTICONS,*/
                                                         /*UnicodeBlock.PLAYING_CARDS,*/
                                                         UnicodeBlock.SUPPLEMENTARY_PRIVATE_USE_AREA_A,
                                                         UnicodeBlock.SUPPLEMENTARY_PRIVATE_USE_AREA_B));
                }
            };
    }
}
