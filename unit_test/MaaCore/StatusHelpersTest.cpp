#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Common/DelayScaler.hpp"
#include "Common/SlashCount.hpp"
#include "Common/TaskReason.hpp"

TEST_CASE("scale_delay_ms keeps zero and identity")
{
    REQUIRE(asst::scale_delay_ms(0, 2.0) == 0);
    REQUIRE(asst::scale_delay_ms(500, 1.0) == 500);
    REQUIRE(asst::scale_delay_ms(500, 2.0) == 1000);
    REQUIRE(asst::scale_delay_ms(1, 2.0) == 2);
}

TEST_CASE("parse_delay_multiplier rejects invalid values")
{
    double value = 0;
    REQUIRE(asst::parse_delay_multiplier("1", value));
    REQUIRE(value == Catch::Approx(1.0));
    REQUIRE(asst::parse_delay_multiplier("2.5", value));
    REQUIRE(value == Catch::Approx(2.5));
    REQUIRE_FALSE(asst::parse_delay_multiplier("", value));
    REQUIRE_FALSE(asst::parse_delay_multiplier("abc", value));
    REQUIRE_FALSE(asst::parse_delay_multiplier("0", value));
    REQUIRE_FALSE(asst::parse_delay_multiplier("0.05", value));
    REQUIRE_FALSE(asst::parse_delay_multiplier("11", value));
}

TEST_CASE("parse_slash_count accepts noisy OCR")
{
    auto parsed = asst::parse_slash_count("370 / 1,800");
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->current == 370);
    REQUIRE(parsed->max == 1800);

    auto full = asst::parse_slash_count("1800/1800");
    REQUIRE(full.has_value());
    REQUIRE(full->current == 1800);
    REQUIRE(full->max == 1800);

    auto empty = asst::parse_slash_count("0/1800");
    REQUIRE(empty.has_value());
    REQUIRE(empty->current == 0);

    REQUIRE_FALSE(asst::parse_slash_count("abc").has_value());
    REQUIRE_FALSE(asst::parse_slash_count("400").has_value());
}

TEST_CASE("pick_weekly_progress prefers 1800 cap over 400-kill record")
{
    const std::vector<asst::SlashCount> hits {
        { .current = 400, .max = 400 },
        { .current = 1110, .max = 1800 },
    };
    auto weekly = asst::pick_weekly_progress(hits);
    REQUIRE(weekly.has_value());
    REQUIRE(weekly->current == 1110);
    REQUIRE(weekly->max == 1800);
}

TEST_CASE("annihilation policy parsers")
{
    asst::OnNoCardAction card = asst::OnNoCardAction::Current;
    REQUIRE(asst::parse_on_no_card("skip", card));
    REQUIRE(card == asst::OnNoCardAction::Skip);
    REQUIRE(asst::parse_on_no_card("fail", card));
    REQUIRE(card == asst::OnNoCardAction::Fail);
    REQUIRE(asst::parse_on_no_card("normal_deploy", card));
    REQUIRE(card == asst::OnNoCardAction::NormalDeploy);
    REQUIRE_FALSE(asst::parse_on_no_card("explode", card));

    asst::OnNoRecordAction record = asst::OnNoRecordAction::Current;
    REQUIRE(asst::parse_on_no_record("skip", record));
    REQUIRE(record == asst::OnNoRecordAction::Skip);
    REQUIRE(asst::is_annihilation_stage("Annihilation"));
    REQUIRE(asst::is_annihilation_stage("Chernobog@Annihilation"));
    REQUIRE_FALSE(asst::is_annihilation_stage("1-7"));
}

TEST_CASE("scale_timeout_seconds")
{
    REQUIRE(asst::scale_timeout_seconds(60, 1.0) == 60);
    REQUIRE(asst::scale_timeout_seconds(60, 2.0) == 120);
    REQUIRE(asst::scale_timeout_seconds(1, 0.1) == 1);
}
